# Post-MVP Vision

## Контекст

MVP библиотеки `cuda_test` завершён (все 7 архивных пакетов приняты). Реализованы модули: `core`, `profiling`, `testing`, `benchmark`, `autotune`, `analysis`, `reporting`. Проведена интеграция с 6 ядрами из тестового движка.

MVP доказал работоспособность подхода, но выявил системные проблемы с UX библиотеки, которые блокируют её практическое использование внешним разработчиком.

## Проблемы MVP

### P1. Избыточный бойлерплейт при описании ядра

Каждый `*Case`-класс в `examples/kernels/kernel_suite.hpp` содержит ~70 строк, из которых ~55 — механический повтор: аллокация `DeviceMemory`, ручное управление `StagedTimer` (8 вызовов `start_*/stop_*`), дублирование `execute()` / `measure()`.

Для добавления нового ядра разработчик вынужден написать полноценный C++ класс, повторяя этот шаблон. Это прямо противоречит цели проекта — минимизировать трудозатраты.

**Влияние:** разработчик тратит больше времени на обвязку, чем на описание самого ядра.

### P2. Отсутствие оркестрации

В MVP нет единой точки входа для полного цикла `тест → бенчмарк → автотюнинг → отчёт`. Разработчик вручную вызывает:
1. `validate()` — проверка корректности
2. `BenchmarkRunner::run()` — бенчмарк
3. `tune_kernel()` — автотюнинг
4. `export_csv()` / `export_json()` — отчёт

Каждый шаг требует отдельной настройки и связывания результатов. Нет пакетного прогона нескольких ядер.

**Влияние:** 4 отдельных действия вместо одного. Нет стандартизированного отчёта «по всем ядрам сразу».

### P3. Несогласованный стиль API

- `BenchmarkRunner` — класс с методом `run()`, а `tune_kernel()` — свободная шаблонная функция.
- `export_csv()` / `export_json()` принимают только `AutoTuneResult`. Экспортировать отдельный `BenchmarkResult` нельзя.
- Модуль `analysis` содержит одну функцию (`transfer_compute_ratio`). Модуль ощущается незаконченным и не отражает возможностей CUDA Runtime API.

**Влияние:** кривая обучения выше, чем необходимо. API не выглядит цельным продуктом.

### P4. Скудная аналитика

Единственная метрика `transfer_compute_ratio` — это начальный уровень. CUDA Runtime API позволяет без Nsight Compute получить: occupancy, kernel function attributes (`numRegs`, local/shared memory usage), bandwidth utilization и оценку memory headroom через `cudaMemGetInfo()`. Эти данные критичны для диагностики, но не собираются.

**Влияние:** разработчик получает числа (median ms), но не понимает *почему* ядро медленное.

### P5. Отсутствие механизма установки

Нет CMake install target, нет `cuda_testConfig.cmake`. Единственный способ использовать библиотеку — скопировать `include/` вручную. Это неприемлемо для production-использования.

**Влияние:** невозможно подключить через `FetchContent`, `find_package`, или пакетный менеджер.

### P6. Отсутствие наглядных отчётов

CSV/JSON хороши для программной обработки, но не дают визуального представления. Для включения в записку, презентацию или быстрого анализа нужен наглядный формат.

**Влияние:** разработчик вынужден строить графики вручную или писать скрипты постобработки.

## Архитектурное решение: три слоя API

```
┌─────────────────────────────────────────────────────┐
│  Layer 3 — Pipeline                                 │
│  KernelDescriptor · Pipeline · Suite                │
│  «Опиши ядро — получи отчёт»                       │
├─────────────────────────────────────────────────────┤
│  Layer 2 — Components (текущий MVP)                 │
│  BenchmarkRunner · tune_kernel · export_*           │
│  compute_stats · StagedTimer · metrics              │
├─────────────────────────────────────────────────────┤
│  Layer 1 — Primitives (текущий MVP)                 │
│  DeviceMemory · CUDA_CHECK · types · DeviceInfo     │
└─────────────────────────────────────────────────────┘
```

- **Layer 1** — остаётся как есть. Низкоуровневые RAII-обёртки и типы.
- **Layer 2** — рефакторинг export для универсальности; расширение analysis до полноценного набора метрик.
- **Layer 3** — новый слой. Декларативное описание ядра (`KernelDescriptor`), оркестрация полного цикла (`Pipeline`), пакетный прогон (`Suite`).

Разработчик в 90% случаев работает с Layer 3. Опытные пользователи могут спуститься на Layer 2/1 для кастомизации.

## Целевой UX

### До (MVP)

```cpp
// ~70 строк на каждое ядро: класс, DeviceMemory поля, конструктор,
// validate(), measure() с ручным StagedTimer, execute()...
class DensityUpdateCase { /* ... 70 lines ... */ };

// Затем вручную:
DensityUpdateCase tc(fixture);
tc.validate(config);

BenchmarkRunner runner({5, 30});
auto bench = runner.run([&]() { return tc.measure(config); });

AutoTuneSpec spec;
spec.block_sizes = {64, 128, 256};
// ...
auto tune = tune_kernel(spec, N, 
    [&](auto cfg) { return tc.measure(cfg); },
    [&](auto cfg) { return tc.validate(cfg); });

export_csv("result.csv", tune);
export_json("result.json", tune);
```

### После (Post-MVP)

```cpp
// ~15 строк: описываем ядро декларативно
auto desc = cuda_test::describe_kernel("density_update")
    .problem_size(N)
    .inputs([&]() { return std::make_tuple(density, delta); })
    .expected([&]() { return expected; })
    .tolerance(1e-5f)
    .launch([](auto config, auto& inputs, auto& output) {
        auto& [d_density, d_delta] = inputs;
        density_update_kernel<<<config.grid, config.block>>>(
            d_density.data(), d_delta.data(), output.data(), N);
    });

// Одна команда — полный цикл
auto report = cuda_test::pipeline(desc)
    .correctness()
    .benchmark()
    .autotune({64, 128, 256, 512})
    .diagnose()   // автоматические рекомендации
    .run();

report.to_csv("result.csv");
report.to_json("result.json");
report.to_html("result.html");

// Пакетный прогон нескольких ядер
auto suite_report = cuda_test::suite("my_project")
    .add(density_desc)
    .add(physics_desc)
    .add(contact_desc)
    .run_all();
```

## Доступные метрики CUDA Runtime API (без Nsight)

| Метрика | API | Назначение |
|---------|-----|-----------|
| Occupancy | `cudaOccupancyMaxActiveBlocksPerMultiprocessor()` | Степень загрузки SM |
| Registers per thread | `cudaFuncGetAttributes()` | Диагностика register pressure |
| Shared mem per block | `cudaFuncGetAttributes()` | Ограничения по shared memory |
| Local memory bytes | `cudaFuncGetAttributes()` | Косвенная диагностика spills / local memory pressure |
| Bandwidth utilization | `bytes / (time_s * theoretical_bw)` | Memory-bound ли ядро |
| Free/total VRAM | `cudaMemGetInfo()` | Оценка memory headroom, но не точная причина slowdown |
| Scaling exponent | `log(t) vs log(N)` по нескольким размерам | Характер масштабирования |
| Block sensitivity | `max(median) / min(median)` по кандидатам | Чувствительность к block size |

Недоступно без Nsight Compute: ALU utilization %, cache hit rates, warp execution efficiency, instruction counts. Это за пределами НИР (§4 записки).

## Система рекомендаций

Rule-based система на основе «слепка ядра» (KernelFingerprint):

```cpp
struct KernelFingerprint {
    double transfer_compute_ratio;  // (h2d + d2h) / kernel
    double occupancy;               // [0..1]
    double bandwidth_utilization;   // [0..1]
    double cv;                      // coefficient of variation
    double block_sensitivity;       // max(median) / min(median)
    double scaling_exponent;        // наклон log-log
};
```

| Паттерн | Условие | Рекомендация |
|---------|---------|-------------|
| Transfer-dominated | `tcr > 3.0` | Ядро слишком лёгкое. Используйте pinned memory, объедините этапы. |
| Low occupancy | `occ < 0.5, regs > 32` | Register pressure. Используйте `__launch_bounds__`. |
| Bandwidth-bound | `bw_util > 0.8` | Упирается в память. Оптимизируйте coalescing, shared memory. |
| Unstable timing | `cv > 0.15` | Высокая дисперсия. Проверьте фоновую нагрузку GPU. |
| Block-sensitive | `block_sens > 1.3` | Запускайте автотюнинг после каждого изменения ядра. |
| Superlinear scaling | `exponent > 1.2` | Contention. Рассмотрите atomics, bank conflicts. |
| Well-utilized | `tcr < 0.3, occ > 0.7` | GPU загружен хорошо. Дальнейшая оптимизация — ILP, warp primitives. |

## Задачи Post-MVP

| # | Задача | Модуль | Решает проблему | Зависимости | Приоритет |
|---|--------|--------|----------------|-------------|-----------|
| 1 | kernel-descriptor | `pipeline` (новый) | P1 бойлерплейт | — | Высокий |
| 2 | pipeline-suite | `pipeline` (новый) | P2 оркестрация | 1 | Высокий |
| 3 | export-refactor | `reporting` | P3 несогласованность | 2 | Высокий |
| 4 | analysis-expansion | `analysis` | P4 скудная аналитика | — | Средний |
| 5 | cmake-install | CMake/infra | P5 установка | — | Средний |
| 6 | diagnostics-advisor | `analysis` | P4 диагностика | 4 | Средний |
| 7 | html-export | `reporting` | P6 наглядность | 2, 6 | Низкий |

Задачи 1-3 — первоочередные (UX). Задачи 4-5 — параллельный трек. Задачи 6-7 — после аналитики.

## Границы модулей

Новый модуль `pipeline` добавляется к существующим 7 модулям:

`core` · `testing` · `benchmark` · `autotune` · `profiling` · `analysis` · `reporting` · **`pipeline`**

Заголовки: `include/cuda_test/pipeline/`. Исходники: `src/pipeline/`.

Решение о добавлении модуля обосновано: `pipeline` — это клей между существующими модулями, он не расширяет ни один из них, а объединяет их в единый workflow.

## Риски

1. **Шаблонная сложность KernelDescriptor.** Декларативный API требует variadic templates и type erasure. Mitigation: ограничить API tuple-based входами, не гнаться за произвольной сигнатурой ядра.
2. **Compile-time overhead.** Header-only + тяжёлые шаблоны = долгая компиляция. Mitigation: forward declarations, extern template где возможно.
3. **Occupancy API требует указатель на `__global__` функцию.** `cudaOccupancyMaxActiveBlocksPerMultiprocessor` принимает `const void*` на ядро. Это усложняет type-erased descriptor. Mitigation: occupancy считается отдельно, не внутри descriptor.

# Feature Packet

## Task

- Name: MVP — минимально жизнеспособная библиотека cuda_test
- Owner: GPT (lead), Claude (reviewer)
- Branch: задачи идут в индивидуальных ветках `task/<phase>-<name>`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §1–§9 (концептуальная), §10–§16 (техническая), §18 (приёмка)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 1–6, срок 06.04.2026

## Objective

Реализовать сквозной путь библиотеки: от изолированного запуска CUDA-ядра до отчёта с рекомендацией конфигурации. MVP покрывает все 7 модулей (core, testing, benchmark, autotune, profiling, analysis, reporting) на минимальном, но рабочем уровне — достаточном для прогона 6 ядер из курсового проекта на 2 GPU и получения данных для пояснительной записки.

## Scope

- In scope:
  - Базовые типы: `KernelLaunchConfig`, `ProfilingBreakdown`, `RunStats`, `AutoTuneSpec`, `AutoTuneResult`
  - RAII-обёртка `DeviceMemory<T>`, выбор устройства, макросы обработки ошибок CUDA
  - `KernelTestFixture` + хелперы валидации (tolerance-based сравнение массивов)
  - Этапный таймер на CUDA Events (H2D, kernel, D2H, total)
  - Benchmark-раннер: warm-up + N measured runs + вычисление RunStats
  - Autotune: генерация кандидатов по block_size, grid search, ранжирование (median → p95 → cv)
  - Производные метрики: `transfer_compute_ratio`, `cv`
  - CSV/JSON экспорт результатов
  - Интеграция 6 ядер из курсового проекта ТООП
  - Сбор evidence на 2 GPU

- Out of scope:
  - DSL-макросы типа `CUDA_EXPECT_ARRAY_NEAR` (удобство, не MVP)
  - `coalescing_sensitivity` и `scaling_exponent` (требуют доп. fixture-наборов)
  - Интеграция с Nsight / CUPTI
  - Автоматическое определение узких мест с рекомендациями
  - Поддержка multi-GPU в одном прогоне
  - Install-target и пакетирование

## Affected Areas

- Modules: core, testing, benchmark, autotune, profiling, analysis, reporting
- Public headers: `include/cuda_test/<module>/` — все модули получат минимум один заголовок
- Internal components: `src/<module>/` — реализации
- Reports or methodology: `reports/` — результаты экспериментов; `docs/methodology/` — без изменений

## Interface Notes

Ключевые типы и функции (из §12–§13 пояснительной записки):

```cpp
// core
struct KernelLaunchConfig { dim3 grid; dim3 block; size_t shared_mem; int device_id; };
template<typename T> class DeviceMemory;  // RAII cudaMalloc/cudaFree
void check_cuda(cudaError_t err);         // throws on error

// profiling
struct ProfilingBreakdown { double h2d_ms, kernel_ms, d2h_ms, total_ms; };
class StagedTimer;  // start/stop per stage via cudaEvent

// testing
class KernelTestFixture : public ::testing::Test;  // setup device, memory
template<typename T>
void expect_array_near(const T* actual, const T* expected, size_t n, T eps);

// benchmark
struct RunStats { double mean_ms, median_ms, p95_ms, ci95_low, ci95_high, cv; };
RunStats compute_stats(const std::vector<double>& samples);

// autotune
struct AutoTuneSpec { std::vector<int> block_sizes; int warmup_runs; int measure_runs; };
struct AutoTuneResult { KernelLaunchConfig best; RunStats stats; std::vector<CandidateRecord> all; std::string reason; };
AutoTuneResult tune_kernel(const AutoTuneSpec& spec, ...);

// analysis
double transfer_compute_ratio(const ProfilingBreakdown& b);

// reporting
void export_csv(const std::filesystem::path& path, const AutoTuneResult& result);
void export_json(const std::filesystem::path& path, const AutoTuneResult& result);
```

## Phases and Task Breakdown

### Phase 1 — Foundation (`task/core-types`)

| Что | Артефакты |
|---|---|
| `KernelLaunchConfig`, `ProfilingBreakdown`, `RunStats` | `include/cuda_test/core/types.hpp` |
| `DeviceMemory<T>` RAII-обёртка | `include/cuda_test/core/device_memory.cuh` |
| `check_cuda()` + макрос `CUDA_CHECK` | `include/cuda_test/core/error.cuh` |
| Device query helper | `include/cuda_test/core/device_info.cuh` |
| Unit-тесты на host-типы | `tests/unit/core/` |

Зависимости: нет.

### Phase 2 — Profiling (`task/profiling-timer`)

| Что | Артефакты |
|---|---|
| `StagedTimer` на CUDA Events | `include/cuda_test/profiling/staged_timer.cuh`, `src/profiling/` |
| Заполнение `ProfilingBreakdown` | через StagedTimer |
| Unit-тест таймера (smoke, ≥0 ms) | `tests/unit/profiling/` |

Зависимости: Phase 1 (types).

### Phase 3 — Testing (`task/testing-fixture`)

| Что | Артефакты |
|---|---|
| `KernelTestFixture` базовый класс | `include/cuda_test/testing/kernel_test_fixture.cuh` |
| `expect_array_near`, `expect_array_eq` | `include/cuda_test/testing/validation.hpp` |
| Пример теста с тривиальным ядром (vector add) | `tests/unit/testing/` |

Зависимости: Phase 1 (DeviceMemory, types).

### Phase 4 — Benchmark + Stats (`task/benchmark-runner`)

| Что | Артефакты |
|---|---|
| `BenchmarkRunner` (warm-up + measure + collect) | `include/cuda_test/benchmark/`, `src/benchmark/` |
| `compute_stats()` → `RunStats` | `include/cuda_test/benchmark/stats.hpp`, `src/benchmark/` |
| Unit-тест stats на синтетических данных | `tests/unit/benchmark/` |

Зависимости: Phase 1 (types), Phase 2 (StagedTimer).

### Phase 5 — Autotune (`task/autotune-search`)

| Что | Артефакты |
|---|---|
| `AutoTuneSpec`, `AutoTuneResult`, `CandidateRecord` | `include/cuda_test/autotune/` |
| `tune_kernel()` — grid search + ranking | `src/autotune/` |
| Unit-тест ranking-логики на mock-данных | `tests/unit/autotune/` |

Зависимости: Phase 4 (BenchmarkRunner, RunStats).

### Phase 6 — Analysis + Reporting (`task/analysis-reporting`)

| Что | Артефакты |
|---|---|
| `transfer_compute_ratio()` | `include/cuda_test/analysis/metrics.hpp` |
| `export_csv()`, `export_json()` | `include/cuda_test/reporting/`, `src/reporting/` |
| Unit-тесты: метрика + round-trip CSV/JSON | `tests/unit/analysis/`, `tests/unit/reporting/` |

Зависимости: Phase 1 (types).

### Phase 7 — Kernel Integration (`task/kernel-integration`)

| Что | Артефакты |
|---|---|
| Адаптеры для 6 ядер из курсового ТООП | `examples/kernels/` |
| Тесты корректности для каждого ядра | `tests/integration/` |
| Autotune прогон + evidence | `benchmarks/kernels/`, `reports/` |
| Прогон на 2 GPU, фиксация результатов | `reports/` |

Зависимости: Phase 1–6 (вся библиотека).

## Dependency Graph

```
Phase 1 (core-types)
├── Phase 2 (profiling-timer)
│   └── Phase 4 (benchmark-runner)
│       └── Phase 5 (autotune-search)
├── Phase 3 (testing-fixture)
└── Phase 6 (analysis-reporting)

Phases 2–6 → Phase 7 (kernel-integration)
```

Phase 3 и Phase 6 не зависят друг от друга и могут идти параллельно.

## Risks

- Technical risks:
  - Курсовые ядра могут требовать нетривиальных fixture (сложные структуры данных) — mitigation: начать с простейшего ядра, наращивать.
  - CUDA Toolkit версия может не совпасть с драйвером на втором GPU — mitigation: проверить `nvidia-smi` заранее.
- Measurement risks:
  - Нестабильные замеры на загруженной системе — mitigation: warm-up, 30 runs, CI95, контроль cv.
- Integration risks:
  - Сроки (06.04.2026): 6 дней на 7 фаз — mitigation: Phase 3 и Phase 6 параллельно; Phase 7 начинать как только Phase 5 готова, не ждать идеального reporting.

## Acceptance Criteria

- [ ] Все 7 модулей имеют минимум один публичный заголовок и хотя бы один unit-тест
- [ ] `cmake --preset msvc-cuda` + build проходит без ошибок
- [ ] Тесты корректности проходят для 6 ядер из курсового проекта
- [ ] Для каждого ядра есть autotune-прогон с выбором лучшей конфигурации
- [ ] Этапные времена (H2D, kernel, D2H, total) замерены для каждого ядра
- [ ] Результаты собраны на 2 GPU
- [ ] CSV/JSON отчёты лежат в `reports/` и содержат RunStats + ProfilingBreakdown
- [ ] 95% доверительный интервал рассчитан для каждого сценария

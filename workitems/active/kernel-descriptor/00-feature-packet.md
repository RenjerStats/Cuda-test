# Feature Packet

## Task

- Name: Post-MVP Phase 1 — KernelDescriptor
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/kernel-descriptor`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` — решает проблему P1

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §11 (архитектура), §12 (публичные интерфейсы), §14 (подход к unit-тестированию), §16 (интеграция)
- Related section(s) in vision: «Целевой UX — После», «P1 — Избыточный бойлерплейт»

## Objective

Создать декларативный builder-интерфейс `KernelDescriptor`, позволяющий описать CUDA-ядро в ~15 строк вместо ~70. Descriptor инкапсулирует: фабрику входных данных, фабрику эталона, функцию запуска ядра, параметры валидации. Библиотека берёт на себя аллокацию `DeviceMemory`, управление `StagedTimer`, копирование H2D/D2H.

Это фундаментальный блок для всех последующих задач (Pipeline, Suite, диагностика).

## Scope

- In scope:
  - Новый модуль `pipeline` с заголовком `include/cuda_test/pipeline/kernel_descriptor.hpp`
  - Builder-класс `KernelDescriptorBuilder` с fluent API:
    - `.problem_size(N)` — размер задачи
    - `.inputs(factory)` — callable, возвращающий `std::tuple<std::vector<T>...>` хостовых буферов
    - `.expected(factory)` — callable, возвращающий `std::vector<T>` эталонного результата
    - `.tolerance(eps)` — float tolerance (по умолчанию — exact match)
    - `.launch(callable)` — callable с сигнатурой `(KernelLaunchConfig, DeviceInputs&, DeviceMemory<OutT>&) -> void`
    - `.build()` → `KernelDescriptor`
  - Свободная функция `describe_kernel(name)` → `KernelDescriptorBuilder`
  - Type-erased `KernelDescriptor` с методами:
    - `name()` → `const std::string&`
    - `problem_size()` → `std::size_t`
    - `validate(config)` → `bool` — полный цикл: inputs → H2D → launch → D2H → compare
    - `measure(config)` → `ProfilingBreakdown` — полный цикл с автоматическим `StagedTimer`
    - `baseline_config(device_id)` → `KernelLaunchConfig` — дефолтный конфиг (block=128)
  - unit-тесты с mock-ядром (host-side) для проверки builder API и lifecycle
  - CUDA-тесты с простым ядром (vector_add) для проверки реального цикла H2D → launch → D2H

- Out of scope:
  - Pipeline/Suite оркестрация (задача 2)
  - Многомерные ядра (2D/3D grid/block) — только 1D в первой итерации
  - Occupancy API — требует указатель на `__global__` функцию, будет добавлен в analysis-expansion
  - Экспорт результатов

## Affected Areas

- Modules: `pipeline` (новый)
- Public headers:
  - `include/cuda_test/pipeline/kernel_descriptor.hpp`
- Internal components:
  - `src/pipeline/` (если потребуются `.cpp` для type erasure)
  - `src/CMakeLists.txt` — добавить pipeline target
- Existing modules: без изменений. `KernelDescriptor` использует `core`, `profiling` как зависимости, не модифицируя их.

## Interface Notes

### Новые типы

```cpp
namespace cuda_test::pipeline {

// Builder — fluent API для конструирования descriptor
template <typename InputFactory, typename ExpectedFactory, typename LaunchFn>
class KernelDescriptorBuilder;

// Type-erased descriptor — хранит всё необходимое для validate/measure
class KernelDescriptor {
public:
    const std::string& name() const noexcept;
    std::size_t problem_size() const noexcept;

    bool validate(const core::KernelLaunchConfig& config);
    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config);
    core::KernelLaunchConfig baseline_config(int device_id = 0) const;
};

// Entry point
KernelDescriptorBuilder</*unset*/> describe_kernel(std::string name);

} // namespace cuda_test::pipeline
```

### Контракт callable-параметров

- `InputFactory`: `() -> std::tuple<std::vector<T1>, std::vector<T2>, ...>`
  Каждый `vector<Ti>` — один хостовый буфер. Размер каждого вектора должен быть равен `problem_size()`. Библиотека создаёт соответствующие `DeviceMemory<Ti>`.

- `ExpectedFactory`: `() -> std::vector<OutT>`
  Эталонный результат на хосте. Размер = `problem_size()`.

- `LaunchFn`: `(const KernelLaunchConfig&, DeviceInputs&, DeviceMemory<OutT>&) -> void`
  Где `DeviceInputs` — tuple из `DeviceMemory<Ti>&`. Разработчик в этом callable вызывает `<<<grid, block>>>` и `cudaGetLastError()`. Синхронизацию (`cudaDeviceSynchronize`) делает библиотека.

### Автоматизация внутри validate() / measure()

Библиотека выполняет автоматически:
1. Вызов `InputFactory` → получение хостовых данных
2. Аллокация `DeviceMemory` для каждого входа и для выхода
3. `StagedTimer::start_total()` + `start_h2d()`
4. Копирование H2D всех входов
5. `stop_h2d()` + `start_kernel()`
6. Вызов `LaunchFn`
7. `cudaDeviceSynchronize()`
8. `stop_kernel()` + `start_d2h()`
9. Копирование D2H выхода
10. `stop_d2h()` + `stop_total()`
11. Для `validate()`: сравнение с эталоном через tolerance/exact
12. Для `measure()`: возврат `ProfilingBreakdown`

### Пример использования

```cpp
constexpr std::size_t N = 1024;
std::vector<float> a(N, 1.0f), b(N, 2.0f), expected(N, 3.0f);

auto desc = cuda_test::pipeline::describe_kernel("vector_add")
    .problem_size(N)
    .inputs([&]() { return std::make_tuple(a, b); })
    .expected([&]() { return expected; })
    .tolerance(1e-5f)
    .launch([N](const auto& config, auto& inputs, auto& output) {
        auto& [d_a, d_b] = inputs;
        vector_add_kernel<<<config.grid, config.block>>>(
            d_a.data(), d_b.data(), output.data(), N);
        CUDA_CHECK(cudaGetLastError());
    });

// Проверка корректности
auto cfg = desc.baseline_config();
bool ok = desc.validate(cfg);

// Измерение с автоматическим таймером
auto breakdown = desc.measure(cfg);
```

## Risks

- **Шаблонная сложность.** Type erasure для произвольных tuple-типов может привести к громоздкому коду. Mitigation: ограничить до `std::tuple<std::vector<T>...>` и `std::vector<OutT>`, не поддерживать произвольные контейнеры.
- **Compile-time overhead.** Каждый descriptor инстанцирует шаблоны. Mitigation: builder шаблонный, но `KernelDescriptor` — type-erased класс с `std::function`/`std::any`, что изолирует шаблонный код в точке `.build()`.
- **Lifetime inputs.** `InputFactory` / `ExpectedFactory` могут захватывать ссылки на данные, которые к моменту вызова уже уничтожены. Mitigation: документировать, что factory вызывается синхронно внутри `validate()`/`measure()` — данные должны быть живы на момент вызова.

## Acceptance Criteria

- [ ] `describe_kernel("name")` возвращает builder с fluent API
- [ ] Builder требует все обязательные поля: `problem_size`, `inputs`, `expected`, `launch`; компиляция без любого из них — ошибка (static_assert или build() не компилируется)
- [ ] `KernelDescriptor::validate(config)` выполняет полный цикл H2D → launch → sync → D2H → compare и возвращает `bool`
- [ ] `KernelDescriptor::measure(config)` выполняет полный цикл с `StagedTimer` и возвращает `ProfilingBreakdown` с ненулевыми значениями
- [ ] `baseline_config()` возвращает конфиг с `block=128`, корректным `grid` для заданного `problem_size`
- [ ] Exact match работает для int-типов (без `.tolerance()`)
- [ ] Near match работает для float-типов (с `.tolerance(eps)`)
- [ ] Host-only unit-тесты проверяют builder API, name, problem_size, baseline_config
- [ ] CUDA-тест с vector_add подтверждает корректность validate() и measure()
- [ ] `measure()` возвращает breakdown, где все 4 этапа > 0 для нетривиального размера входа

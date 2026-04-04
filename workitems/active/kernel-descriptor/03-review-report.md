# Review Report

## Linkage

- Feature packet: `workitems/active/kernel-descriptor/00-feature-packet.md`
- Test contract: `workitems/active/kernel-descriptor/01-test-contract.md`
- Implementation notes: `workitems/active/kernel-descriptor/02-implementation-notes.md`
- Reviewer: Claude (Opus)
- Review date: 2026-04-04

## Review Surface

Uncommitted changes on branch `task/kernel-descriptor` relative to `248ff13` (HEAD of MVP):

| File | Type | Lines |
|------|------|-------|
| `include/cuda_test/pipeline/kernel_descriptor.hpp` | NEW | 532 |
| `include/cuda_test/cuda_test.hpp` | MOD | +1 (pipeline include) |
| `tests/CMakeLists.txt` | MOD | +2 (test targets) |
| `tests/unit/pipeline/kernel_descriptor_test.cpp` | NEW | 106 |
| `tests/integration/pipeline/kernel_descriptor_cuda_test.cu` | NEW | 177 |
| `tests/fixtures/vector_add_fixture.hpp` | NEW | 31 |

## Verdict: APPROVED

Архитектура добротная, код чистый, тесты адекватны. Все acceptance criteria из пакета выполнены. Найденные замечания — не блокеры, но два major-замечания требуют внимания при реализации pipeline-suite.

---

## Spec Compliance

| Acceptance Criterion | Status | Evidence |
|---------------------|--------|---------|
| `describe_kernel("name")` → builder с fluent API | pass | `kernel_descriptor.hpp:517-519` |
| Builder требует все обязательные поля | pass | `static_assert` (строки 393-395, 412-413, 473-479) + runtime throw (строки 443-462) + тест `BuildThrowsWhenRequiredFieldsAreMissing` |
| `validate(config)` — полный цикл H2D→launch→D2H→compare | pass | `KernelDescriptorModel::validate()` → `run_once()` (строки 300-311, 330-363) + CUDA тест `ValidateReturnsTrueForMatchingFloatKernel` |
| `measure(config)` — полный цикл с StagedTimer | pass | `run_once()` с автоматическим таймером + CUDA тест `MeasureReturnsPositiveBreakdownForNonTrivialInput` |
| `baseline_config()` → block=128, корректный grid | pass | `make_baseline_config()` (строки 185-198) + тесты `BaselineConfigUsesDefaultBlockAndCoversProblem`, `BaselineConfigRoundsUpForNonAlignedProblemSizes` |
| Exact match для int (без tolerance) | pass | `ValidateUsesExactMatchForIntegralOutputs` |
| Near match для float (с tolerance) | pass | `ValidateReturnsTrueForMatchingFloatKernel` (tolerance=1e-5) |
| Host-only unit-тесты | pass | 5 тестов + 1 skipped (GPU check) в `kernel_descriptor_test.cpp` |
| CUDA-тесты с vector_add | pass | 8 тестов в `kernel_descriptor_cuda_test.cu` |
| `measure()` → breakdown со всеми 4 этапами > 0 | pass | `MeasureReturnsPositiveBreakdownForNonTrivialInput` (N=65536) |

## Test Contract Compliance

| Contract Scenario | Covered | Test Name |
|------------------|---------|-----------|
| Builder fluent API — all fields | yes | `BuildsDescriptorAndExposesMetadata` |
| Builder — missing field | yes | `BuildThrowsWhenRequiredFieldsAreMissing` |
| `baseline_config()` default | yes | `BaselineConfigUsesDefaultBlockAndCoversProblem` |
| `baseline_config()` non-aligned | yes | `BaselineConfigRoundsUpForNonAlignedProblemSizes` |
| `validate()` — correct kernel | yes | `ValidateReturnsTrueForMatchingFloatKernel` |
| `validate()` — wrong expected | yes | `ValidateReturnsFalseForWrongExpectedFloatKernel` |
| `validate()` — exact match int | yes | `ValidateUsesExactMatchForIntegralOutputs` |
| `measure()` — non-trivial | yes | `MeasureReturnsPositiveBreakdownForNonTrivialInput` |
| `measure()` — stages ordered | yes | `MeasureKeepsTotalStageGreaterThanComponentStages` |
| `measure()` — repeated calls | yes | `MeasureCanBeCalledRepeatedlyOnTheSameDescriptor` |
| Descriptor move semantics | partial | `MoveConstructionPreservesMetadata` — проверяет name/problem_size/baseline_config, но не validate/measure после move (host-only тест, оправдано) |
| Zero problem size | yes | `ValidateReturnsTrueForZeroProblemSize` (и validate, и measure) |
| Kernel launch error | yes | `MeasurePropagatesInvalidLaunchConfiguration` |
| Device unavailable | yes | `BaselineConfigRejectsUnavailableDeviceWhenRuntimeSeesGPUs` |

---

## Findings

### M1 — major: Аллокация device-памяти при каждом вызове validate/measure

**Файл:** `kernel_descriptor.hpp:334-335`

```cpp
DeviceBuffers device_inputs = make_device_buffers(host_inputs);
core::DeviceMemory<OutputValue> output_device(problem_size_);
```

Каждый вызов `validate()` или `measure()` выполняет `cudaMalloc` для каждого входного буфера + выходной буфер, затем `cudaFree` при выходе из scope. Для vector_add с 2 входами + 1 выход = 3 пары malloc/free на вызов.

При использовании через `BenchmarkRunner` (warmup=5 + measured=30 = 35 итераций) или `tune_kernel` (6 кандидатов × 35 = 210 итераций) это 630-1050+ пар `cudaMalloc`/`cudaFree`, каждая из которых может стоить >1ms.

Аллокация происходит **до** запуска таймера, поэтому не влияет на точность профилирования, но создаёт значительный overhead на общее время прогона.

**Рекомендация:** При реализации `pipeline-suite` (задача 2) рассмотреть caching-слой в Pipeline, который аллоцирует буферы однократно и передаёт их в descriptor для повторного использования. Вариант: добавить в `KernelDescriptor` перегрузку `measure(config, pre_allocated_buffers)`. Не требует изменений в текущей задаче — descriptor как самостоятельная единица работает корректно.

---

### M2 — major: `mutable` на factory-членах + `shared_ptr` = thread-safety concern

**Файл:** `kernel_descriptor.hpp:367-369`

```cpp
mutable InputFactory input_factory_;
mutable ExpectedFactory expected_factory_;
mutable LaunchFn launch_fn_;
```

`KernelDescriptor` хранит `shared_ptr<const Concept>`, что делает его copyable. Копии разделяют один `Model`. `mutable` позволяет вызывать factory из `const` методов, но это означает, что два потока, вызывающие `validate()` на копиях одного descriptor, могут столкнуться на `mutable` состоянии.

Для текущего use case (однопоточный, один GPU) проблемы нет. Spec явно указывает: Suite — последовательный прогон.

**Рекомендация:** Документировать в заголовке: `KernelDescriptor` is not thread-safe for concurrent `validate()`/`measure()` calls on shared instances. Это стандартная практика для CUDA-кода. Не требует исправления сейчас.

---

### m1 — minor: `tie_device_buffers` внутри kernel-timing секции

**Файл:** `kernel_descriptor.hpp:343-345`

```cpp
timer.start_kernel();
DeviceInputRefs device_input_refs = tie_device_buffers(device_inputs);
std::invoke(launch_fn_, config, device_input_refs, output_device);
```

`tie_device_buffers` — тривиальная операция (создание tuple ссылок), overhead ~наносекунды. Но формально она входит в kernel timing.

**Рекомендация:** Вынести `tie_device_buffers` перед `start_kernel()`. Одна строка, ноль риска:

```cpp
DeviceInputRefs device_input_refs = tie_device_buffers(device_inputs);
timer.start_kernel();
std::invoke(launch_fn_, config, device_input_refs, output_device);
```

---

### m2 — minor: Проверка tolerance vs arithmetic в build() — мёртвая ветка при compile-time

**Файл:** `kernel_descriptor.hpp:481`

```cpp
if (tolerance_.has_value() && !std::is_arithmetic_v<OutputValue>) {
    throw std::invalid_argument(...);
}
```

`std::is_arithmetic_v<OutputValue>` — constexpr. Для arithmetic типов эта ветка никогда не выполняется. Для non-arithmetic — всегда выполняется (если tolerance задан). Компилятор оптимизирует, но код выглядит как runtime-проверка compile-time факта.

**Рекомендация:** Заменить на:

```cpp
if constexpr (!std::is_arithmetic_v<OutputValue>) {
    if (tolerance_.has_value()) {
        throw std::invalid_argument(...);
    }
}
```

Чистая косметика, не влияет на поведение.

---

### m3 — minor: Convenience forwarding в корневой namespace

**Файл:** `kernel_descriptor.hpp:523-531`

```cpp
namespace cuda_test {
using pipeline::KernelDescriptor;
[[nodiscard]] inline auto describe_kernel(std::string name) { ... }
}
```

Задокументировано в implementation notes как отклонение. Решение разумное — приближает API к целевому виду из vision. Потенциальный ADL-конфликт при одновременном `using namespace cuda_test` и `using namespace cuda_test::pipeline` — теоретический, на практике не возникнет (пользователь будет использовать одно или другое).

**Рекомендация:** Оставить как есть. При добавлении `Pipeline` и `Suite` в задаче pipeline-suite — использовать тот же паттерн для консистентности.

---

### m4 — minor: Тест move semantics не проверяет функциональность после move

**Файл:** `kernel_descriptor_test.cpp:72-87`

Тест `MoveConstructionPreservesMetadata` проверяет `name()`, `problem_size()`, `baseline_config()` после move, но не `validate()` или `measure()`. Это оправдано — host-only тест не может вызвать CUDA. Но в CUDA тестах функциональность после move тоже не проверена.

**Рекомендация:** Не блокирует. `shared_ptr` move гарантирует сохранность — это поведение STL, не нуждающееся в доказательстве. Однако при желании можно добавить один CUDA тест: move descriptor → validate → expect true.

---

### m5 — minor: Factory вызывается при каждом validate/measure — повторные аллокации host-данных

**Файл:** `kernel_descriptor.hpp:301-302, 315-316`

Каждый `validate()` и `measure()` вызывает `std::invoke(input_factory_)`, создавая новые host-векторы. Для больших данных (N=1M) × 30 прогонов = 30 аллокаций по нескольку MB.

**Рекомендация:** Аналогично M1 — оптимизация на уровне Pipeline. Pipeline может вызвать factory один раз и передавать результат в descriptor. Текущий дизайн корректен — factory-семантика гарантирует свежие данные.

---

## Architecture Assessment

### Сильные стороны

1. **Concept/Model type erasure** — чистая реализация. `KernelDescriptor` скрывает все шаблонные параметры за runtime-полиморфизмом. Пользователь работает с единым типом `KernelDescriptor`, Pipeline будет принимать его без шаблонов.

2. **Builder с type-level tracking** — `KernelDescriptorBuilder<InputFactory, ExpectedFactory, LaunchFn>` отслеживает установленные поля на уровне типов. `static_assert` в `build()` ловит ошибки при компиляции. `unset_t` sentinel — лаконичное решение.

3. **Автоматизация StagedTimer** — пользователь не касается таймера. Решена проблема P1 из vision.

4. **Detail namespace** — вся внутренняя механика (`is_std_vector`, `device_buffers_tuple`, `validate_input_sizes`, `copy_inputs_to_device`, `vectors_match`) спрятана в `detail`. Чистый публичный API.

5. **Тестовое покрытие** — 13 тестов (5 host + 8 CUDA), покрывающие happy path, negative scenarios, edge cases (zero size), reuse, move semantics. Адекватно для фундаментального компонента.

6. **Совместимость с host-only build** — `#if defined(CUDA_TEST_HAS_CUDA)` guards в нужных местах. Builder и metadata работают без CUDA.

### Совместимость с последующими задачами

| Задача | Готовность |
|--------|-----------|
| pipeline-suite | `KernelDescriptor` предоставляет `validate()`, `measure()`, `baseline_config()` — всё что нужно Pipeline. `shared_ptr` делает descriptor copyable для Suite. |
| export-refactor | Descriptor не экспортирует данные — это ответственность Pipeline/Report. Нет конфликта. |
| analysis-expansion | `measure()` возвращает `ProfilingBreakdown` — входной тип для analysis metrics. Совместимо. |
| diagnostics-advisor | Occupancy API требует `const void*` на kernel — в descriptor этого нет (type-erased). Как задокументировано в spec, occupancy будет вызываться отдельно. Не блокирует. |
| autotune integration | `tune_kernel` принимает callable `(config) -> ProfilingBreakdown` — `[&desc](auto c){ return desc.measure(c); }` подходит. + callable `(config) -> bool` для validator — `[&desc](auto c){ return desc.validate(c); }`. Совместимо. |

---

## Summary

| Severity | Count | IDs |
|----------|-------|-----|
| blocker | 0 | — |
| major | 2 | M1 (device alloc per call), M2 (mutable + shared_ptr thread safety) |
| minor | 5 | m1-m5 |

**Disposition:** Принять. Major замечания M1 и M2 документировать и учитывать при реализации pipeline-suite (M1 — caching; M2 — document thread safety). Minor m1 (tie_device_buffers placement) и m2 (if constexpr) рекомендую исправить в текущей ветке если есть возможность — это одна строка каждое. Остальные minor — информационные.

Реализация создаёт прочный фундамент для Layer 3 (Pipeline/Suite). Архитектура type erasure правильно изолирует шаблонный код от публичного API. Тестовое покрытие адекватно.

# Review Report

## Review Metadata

- Reviewer: Claude (reviewer)
- Reviewed branch or diff: source tree review for `task/profiling-timer`
- Date: 2026-04-03
- Decision: **accept**

## Findings

### Blocker

- None.

### Major

- None.

### Minor

1. **`cuda_runtime.h` вместо `cuda_runtime_api.h` в `cuda_compat.hpp`.**
   Предыдущая фаза использовала `<cuda_runtime_api.h>` (только C API). Теперь подключается `<cuda_runtime.h>`, который тянет за собой host-side launch syntax и дополнительные определения. Это нужно для `.cu` тестов с `<<<>>>`, но compat-заголовок включается и из чистых C++ заголовков. Рекомендация: зафиксировать в комментарии, почему выбран `cuda_runtime.h`, или рассмотреть раздельные include для compat (api-only) и тестов (full runtime).

2. **`StagedTimer` — non-movable.**
   Move-конструктор и move-assignment удалены (`= delete`). Это корректно для RAII вокруг CUDA events, но ограничивает использование в будущих фазах: таймер нельзя хранить в контейнерах или возвращать из фабрик. Если benchmark runner потребует ownership transfer, придётся оборачивать в `unique_ptr`. Рекомендация: если в будущем потребуется move, реализовать по аналогии с `DeviceMemory<T>`.

3. **`stop()` содержит `cudaEventSynchronize` — блокирующий вызов.**
   Каждый `stop()` блокирует host до завершения записи event'а. Для текущей фазы (одиночный default stream) это корректно и упрощает API. Но при переходе к multi-stream координации в будущих фазах это станет узким местом. Рекомендация: задокументировать, что текущий API является synchronous-only.

4. **`coefficient_of_variation` в тесте использует population variance (делит на N, не N-1).**
   Для 10 выборок это занижает оценку на ~5%. При пороге `cv < 0.5` это не влияет на результат, но формально для sample CV правильнее `N-1`. Не блокер.

5. **Warning policy для CUDA: `Warnings.cmake` теперь ограничивает флаги `COMPILE_LANGUAGE:CXX`.**
   Это обоснованное решение (nvcc + MSVC генерируют шум на `/W4`), но стоит добавить комментарий в `Warnings.cmake`, объясняющий почему CUDA compile language исключена из warning flags.

6. **Прямой вызов `core::check_cuda(...)` вместо макроса `CUDA_CHECK`.**
   В `staged_timer.hpp` (строки 42–45, 62–68, 72–75, 145–152) используется `core::check_cuda(...)` с захардкоженной строкой выражения. Та же рекомендация, что в Phase 1: переход на `CUDA_CHECK` устранит риск рассинхронизации строки с реальным выражением.

## Required Changes

- None (все findings классифицированы как minor).
- Follow-up before commit: minor findings were addressed in-place (comments for runtime/warnings semantics, `CUDA_CHECK` adoption in `StagedTimer`, sample CV in tests).

## Соответствие пояснительной записке

Проверено соответствие реализации Phase 2 ключевым разделам пояснительной записки:

| Раздел записки | Требование | Статус |
|---|---|---|
| §5, п.3 | Этапное профилирование (H2D, Kernel, D2H, Total) | **Реализовано.** `StagedTimer` измеряет все 4 этапа через `Stage` enum. |
| §11 | `profiling`: этапные таймеры на CUDA Events, сбор статистики | **Частично.** Таймеры реализованы; сбор статистики (`RunStats`) корректно отложен до benchmark-фазы. |
| §12 | `ProfilingBreakdown` с полями `h2d_ms`, `kernel_ms`, `d2h_ms`, `total_ms` | **Полное соответствие.** Структура из Phase 1 используется как есть. |
| §15 | Метрики `t_h2d`, `t_kernel`, `t_d2h`, `t_total` | **Реализовано.** Имена полей совпадают с записко1й. |
| §15 | Производные показатели (`transfer_compute_ratio`, `coalescing_sensitivity`, `scaling_exponent`) | **Корректно отложено.** Относится к модулю `analysis`, не к `profiling`. |
| §6, п.1 | 30 повторов, 95% CI | **Не применимо к этой фазе.** Consistency test использует 10 повторов — это smoke-проверка стабильности таймера, а не финальное измерение. |
| §13 | Алгоритм автоподбора использует `ProfilingBreakdown` | **Интерфейс готов.** `StagedTimer::breakdown()` возвращает `ProfilingBreakdown`, который будет входом для autotune. |

**Вывод:** реализация Phase 2 точно следует архитектуре из §11, публичным типам из §12 и метрикам из §15. Границы модуля `profiling` не нарушены — производные метрики и статистика корректно оставлены для модулей `analysis` и `benchmark`. Проект идёт в русле пояснительной записки.

## Notes

### Процесс (stage-gate)

Порядок `spec → tests → implementation → review` соблюдён:
- `00-feature-packet.md` — scope, affected areas, acceptance criteria определены.
- `01-test-contract.md` — 5 correctness scenarios + 3 negative scenarios + consistency check.
- `02-implementation-notes.md` — валидация зафиксирована, отклонение (header-only) обосновано.

### Архитектура

- Header-only подход для `StagedTimer` обоснован: класс целиком состоит из RAII-обёрток и inline-логики вокруг CUDA Events. При добавлении тяжёлой логики в будущих фазах можно вынести в compiled target без изменения API.
- `Stage` enum с `std::size_t` underlying type + `to_index()` — чистый mapping в массив, без ветвлений на hot path.
- `cuda_compat.hpp` корректно расширен: добавлены `cudaEvent_t` и `cudaStream_t` fallback types для host-only сборки.

### Тесты

- 6 тестов в `staged_timer_test.cu`:
  - `DefaultBreakdownIsZero` — начальное состояние.
  - `SmokeMeasuresAllStages` — smoke с реальным kernel.
  - `ResetClearsBreakdown` — reset не оставляет stale данных.
  - `ReuseProducesFreshTimings` — повторное использование.
  - `GenericStageApiRejectsInvalidOrdering` — negative: stop без start, double start.
  - `KernelTimingRemainsReasonablyStable` — consistency check (10 runs, cv < 0.5).
- Тест-helper `run_with_named_wrappers` проверяет и таймер, и корректность kernel output — двойная польза.
- `GTEST_SKIP()` при отсутствии GPU через `SetUp()` fixture — корректно.

### Acceptance Criteria

| Критерий | Статус |
|---|---|
| `staged_timer.hpp` — reusable staged timer API | ✓ |
| `StagedTimer` измеряет `h2d`, `kernel`, `d2h`, `total` в `ProfilingBreakdown` | ✓ |
| `reset()` не сохраняет stale timings | ✓ |
| CUDA profiling тесты проходят в `msvc-cuda` | ✓ (16/16) |
| Host-only configure/build валиден | ✓ |

### Решение

**Accept.** Реализация полностью соответствует feature packet, test contract и пояснительной записке. Все acceptance criteria выполнены. Minor findings не блокируют и адресуются в последующих фазах. Проект движется в заданном направлении — profiling-слой готов к интеграции с benchmark/autotune.

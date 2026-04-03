# Review Report

## Review Metadata

- Reviewer: Claude (reviewer)
- Reviewed branch or diff: staged files for `task/core-types` (source tree review)
- Date: 2026-04-03
- Decision: **accept**

## Findings

### Blocker

- None.

### Major

- None.

### Minor

1. **`check_cuda` в `error.hpp` — ручная строка вместо `CUDA_CHECK`.**
   В `device_info.hpp:30` и `device_info.hpp:48–51` вызывается `check_cuda(...)` напрямую с хардкод-строкой выражения вместо макроса `CUDA_CHECK`. Это допустимо технически, но создаёт риск рассинхронизации строки описания с реальным выражением при рефакторинге. Рекомендуется перейти на `CUDA_CHECK(cudaGetDeviceCount(&count))` и `CUDA_CHECK(cudaGetDeviceProperties(&props, device_id))` в будущих фазах.

2. **`cudaFree` в `release()` — ошибка игнорируется.**
   `device_memory.hpp:125` — `(void)cudaFree(data_)` подавляет ошибку. Для деструктора это корректное решение (нельзя бросать из деструктора), но стоит добавить комментарий, поясняющий это решение, чтобы будущие читатели не считали это случайным пропуском. Не блокер.

3. **`device_count()` — специальная обработка `cudaErrorNoDevice`.**
   `device_info.hpp:26–28` — только `cudaErrorNoDevice` обрабатывается отдельно. Другие возможные ошибки (например, `cudaErrorInsufficientDriver`) пробрасываются через `check_cuda`. Это приемлемо, но стоит зафиксировать решение в комментарии.

4. **Отсутствие `error.hpp` include в `core_cuda_runtime_test.cpp`.**
   Тест вызывает `check_cuda(...)` напрямую (строка 15), но include `error.hpp` происходит транзитивно через `device_info.hpp`. Для явности и устойчивости к рефакторингу лучше включить `error.hpp` напрямую.

## Required Changes

- None (все findings классифицированы как minor).

## Notes

### Процесс (stage-gate)

Порядок `spec → tests → implementation → review` соблюдён:
- `00-feature-packet.md` определяет scope, affected areas и acceptance criteria.
- `01-test-contract.md` покрывает все заявленные сценарии (defaults, round-trip, zero-size, move, negative cases, device helpers).
- `02-implementation-notes.md` фиксирует валидацию и отклонения (единственное: добавлен `cuda_compat.hpp`, что обосновано).

### Архитектура

- Решение header-only для `core` — правильный выбор на данном этапе. Все типы — POD-like struct'ы, `DeviceMemory<T>` — шаблон, inline-функции не создают проблем с ODR.
- `cuda_compat.hpp` — грамотный shim: обеспечивает компиляцию host-only пресета без `cuda_runtime.h`, при этом значения fallback-констант соответствуют реальным значениям CUDA API.
- Модульные границы не нарушены: всё находится в `core`, новые модули не введены.

### Тесты

- `core_types_test.cpp` — покрывает default values для всех трёх struct'ов (3 теста).
- `core_cuda_runtime_test.cpp` — 7 тестов: `check_cuda` throw, zero-size buffer, round-trip copy, move semantics, copy mismatch, device exists + device info, invalid device rejection. Покрытие соответствует test contract.
- Корректное использование `GTEST_SKIP()` при отсутствии GPU — тесты не ломаются на host-only окружении.

### Acceptance Criteria

| Критерий | Статус |
|---|---|
| Публичные заголовки `core` под `include/cuda_test/core/` | ✓ |
| `KernelLaunchConfig`, `ProfilingBreakdown`, `RunStats` с default values | ✓ |
| `DeviceMemory<T>` — zero-size, move, round-trip | ✓ |
| `CUDA_CHECK` с контекстом | ✓ |
| Device helper: count + properties | ✓ |
| Unit-тесты в `tests/unit/core/` | ✓ |

### Решение

**Accept.** Реализация полностью соответствует feature packet и test contract. Все acceptance criteria выполнены. Minor findings не блокируют и могут быть адресованы в последующих фазах.

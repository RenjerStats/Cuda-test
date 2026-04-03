# Feature Packet

## Task

- Name: Phase 1 - Foundation / core-types
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/core-types`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §11 (architecture), §12 (public structures and interfaces), §14 (unit testing approach), §18 (acceptance)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 2-4, согласованный стек C++17 + CUDA Runtime API

## Objective

Подготовить базовый `core` слой, на который будут опираться все последующие модули библиотеки. Результат фазы должен дать стабильные типы конфигурации и статистики, безопасную обработку ошибок CUDA, RAII-обёртку для памяти устройства и минимальный helper для выбора/опроса устройства без выхода за границы модуля `core`.

## Scope

- In scope:
  - `KernelLaunchConfig`, `ProfilingBreakdown`, `RunStats`
  - `DeviceMemory<T>` с RAII-семантикой, `size()`, `data()`, H2D/D2H copy helpers
  - `check_cuda()` и макрос `CUDA_CHECK`
  - helper для получения информации об устройстве и проверки доступности устройства
  - unit-тесты для host-friendly типов и CUDA runtime helpers
  - интеграция `core` заголовков в основной library target

- Out of scope:
  - запуск CUDA-ядер и любые `testing` fixture
  - benchmark/autotune/p95/CI вычисления как отдельная логика
  - staged profiling
  - сериализация отчётов
  - интеграция ядер из курсового проекта

## Affected Areas

- Modules: `core`
- Public headers:
  - `include/cuda_test/core/types.hpp`
  - `include/cuda_test/core/error.hpp`
  - `include/cuda_test/core/device_memory.hpp`
  - `include/cuda_test/core/device_info.hpp`
- Internal components:
  - `src/CMakeLists.txt`
  - при необходимости `src/core/`
- Reports or methodology: без изменений

## Interface Notes

- New or changed types:
  - `KernelLaunchConfig` с безопасными default values
  - `ProfilingBreakdown`
  - `RunStats`
  - `DeviceInfo`
- New or changed functions:
  - `void check_cuda(cudaError_t error, const char* expr, const char* file, int line);`
  - `device_count()`, `device_exists(int)`, `get_device_info(int)`
  - `DeviceMemory<T>::copy_from_host(...)`, `copy_to_host(...)`
- Input or output assumptions:
  - zero-size буферы допустимы и не должны приводить к `cudaMalloc(0)`
  - CUDA-dependent API должен компилироваться только когда доступен CUDA toolkit
  - host-only preset не должен ломаться из-за отсутствия `cuda_runtime.h`

## Risks

- Technical risks:
  - прямое включение CUDA runtime в публичные заголовки может сломать host-only build; mitigation: условная совместимость через shim/fallback types и compile definitions
  - `DeviceMemory<T>` может некорректно вести себя при move/reset на empty buffer; mitigation: отдельные unit-тесты на zero-size и move semantics
- Measurement risks:
  - отсутствуют для этой фазы; производительные утверждения не делаются
- Integration risks:
  - сигнатуры `core` типов должны совпасть с будущими фазами `profiling`, `benchmark`, `autotune`; mitigation: держать интерфейс максимально близко к `mvp` packet

## Acceptance Criteria

- [ ] Публичные заголовки `core` добавлены под `include/cuda_test/core/`
- [ ] `KernelLaunchConfig`, `ProfilingBreakdown`, `RunStats` имеют проверяемые default values
- [ ] `DeviceMemory<T>` поддерживает zero-size, move semantics и round-trip host/device copy
- [ ] `CUDA_CHECK` поднимает ошибку с контекстом выражения и места вызова
- [ ] helper по устройству позволяет запросить count и свойства устройства
- [ ] unit-тесты `tests/unit/core/` проходят в доступной конфигурации сборки

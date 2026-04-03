# Feature Packet

## Task

- Name: Phase 2 - Profiling / staged-timer
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/profiling-timer`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §5 (functional requirement for staged profiling), §11 (profiling module), §12 (ProfilingBreakdown), §15 (H2D/Kernel/D2H/Total metrics), §18 (acceptance)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 2, 4, 5; agreed stack with CUDA Runtime API

## Objective

Реализовать минимальный, но пригодный к повторному использованию profiling-слой на базе CUDA Events. Результат фазы должен позволять измерять этапы `H2D`, `kernel`, `D2H` и `total`, получать заполненный `ProfilingBreakdown` и использовать этот интерфейс в будущих benchmark/autotune фазах без привязки к конкретному ядру.

## Scope

- In scope:
  - `StagedTimer` для этапов `h2d`, `kernel`, `d2h`, `total`
  - накопление измерений в `ProfilingBreakdown`
  - reset и повторное использование одного timer instance
  - unit-тесты smoke и consistency на тривиальном CUDA kernel
  - интеграция profiling header в umbrella header и CMake test registration

- Out of scope:
  - вычисление статистики по серии замеров (`RunStats`)
  - benchmark/autotune orchestration
  - multi-stream coordination beyond optional stream parameter forwarding
  - Nsight/CUPTI integration
  - serialization/reporting

## Affected Areas

- Modules: `profiling`
- Public headers:
  - `include/cuda_test/profiling/staged_timer.hpp`
- Internal components:
  - `tests/unit/profiling/`
  - `tests/CMakeLists.txt`
  - `include/cuda_test/cuda_test.hpp`
  - `include/cuda_test/core/detail/cuda_compat.hpp` if extra CUDA compatibility types are needed
- Reports or methodology: no report artifact yet; metric names must stay aligned with `docs/methodology/benchmark-protocol.md`

## Interface Notes

- New or changed types:
  - `enum class Stage { h2d, kernel, d2h, total }` as internal/publicly usable selector
  - `class StagedTimer`
- New or changed functions:
  - `start(Stage stage, cudaStream_t stream = nullptr)`
  - `stop(Stage stage, cudaStream_t stream = nullptr)`
  - named wrappers `start_h2d`, `stop_h2d`, `start_kernel`, `stop_kernel`, `start_d2h`, `stop_d2h`, `start_total`, `stop_total`
  - `reset()`
  - `breakdown() const`
- Input or output assumptions:
  - timer is valid only in CUDA-enabled builds; host-only builds must still compile public headers
  - elapsed values are reported in milliseconds
  - `breakdown()` returns `0.0` for stages that were not fully measured yet

## Risks

- Technical risks:
  - CUDA event lifecycle bugs may leak resources or return invalid timings; mitigation: RAII ownership and reset tests
  - host-only build can break if profiling headers expose raw CUDA-only types without fallback; mitigation: extend compat layer only as much as needed
- Measurement risks:
  - tiny kernels may produce noisy timings close to event resolution; mitigation: smoke test checks only non-negative values and consistency test uses repeated runs with relaxed `cv < 0.5`
- Integration risks:
  - future benchmark runner will depend on this interface; mitigation: keep stage names and breakdown fields identical to NIR wording

## Acceptance Criteria

- [ ] `include/cuda_test/profiling/staged_timer.hpp` provides a reusable staged timer API
- [ ] `StagedTimer` measures `h2d`, `kernel`, `d2h`, `total` into `ProfilingBreakdown`
- [ ] repeated use with `reset()` does not retain stale timings
- [ ] CUDA profiling tests pass in `msvc-cuda`
- [ ] host-only configure/build remains valid after adding profiling public headers

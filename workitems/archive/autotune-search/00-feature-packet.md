# Feature Packet

## Task

- Name: Phase 5 - Autotune / autotune-search
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/autotune-search`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §5 (automated launch selection), §6 (30 measured runs and 95% CI), §11 (autotune module), §12 (`AutoTuneSpec`), §13 (candidate search and ranking), §18 (performance acceptance)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 2, 4, 5; agreed stack with CUDA Runtime API + Google Benchmark

## Objective

Реализовать минимальный autotune-слой, который опирается на `BenchmarkRunner` и умеет искать лучшую конфигурацию запуска среди набора кандидатов. Результат фазы должен дать `AutoTuneSpec`, `CandidateRecord`, `AutoTuneResult` и `tune_kernel()` с детерминированным ранжированием `median -> p95 -> cv`, чтобы следующая фаза интеграции ядер могла использовать уже готовую логику выбора победителя.

## Scope

- In scope:
  - `AutoTuneSpec`, `CandidateRecord`, `AutoTuneResult`
  - candidate generation from `block_sizes` and `grid_wave_multipliers`
  - filtering invalid candidates by thread-block limits before measurement
  - `tune_kernel()` on top of `BenchmarkRunner`
  - explicit winner reason based on `median`, `p95`, `cv`
  - host-only unit tests for candidate generation, filtering, and ranking tie-breakers
  - umbrella-header integration for autotune public API

- Out of scope:
  - real CUDA kernel autotune runs and evidence in `reports/`
  - baseline-vs-winner report export
  - device-occupancy heuristics beyond max threads per block
  - correctness fixture generation for course kernels
  - multi-dimensional launch search beyond 1D grid/block generation

## Affected Areas

- Modules: `autotune`
- Public headers:
  - `include/cuda_test/autotune/search.hpp`
- Internal components:
  - `tests/unit/autotune/`
  - `tests/CMakeLists.txt`
  - `include/cuda_test/cuda_test.hpp`
- Reports or methodology:
  - implementation must stay aligned with `docs/methodology/benchmark-protocol.md`

## Interface Notes

- New or changed types:
  - `struct AutoTuneSpec`
  - `struct CandidateRecord`
  - `struct AutoTuneResult`
- New or changed functions:
  - `tune_kernel(const AutoTuneSpec& spec, std::size_t problem_size, MeasuredRun&& measured_run, Validator&& validator)`
- Input or output assumptions:
  - default policy remains `warmup_runs=5`, `measure_runs=30`, `grid_wave_multipliers={1}`
  - candidate ranking uses `kernel_stats.median_ms`, then `kernel_stats.p95_ms`, then `kernel_stats.cv`
  - `validator(config)` is called before timing; candidates rejected by validator are not measured
  - invalid block sizes above the resolved `max_threads_per_block` are excluded before measurement
  - this phase does not define a baseline configuration; caller-provided candidates are simply ranked

## Risks

- Technical risks:
  - `AutoTuneResult` can drift from later reporting needs; mitigation: keep winner `RunStats` plus full candidate benchmark records
  - candidate generation may overfit the current 1D search shape; mitigation: explicitly limit this phase to 1D config generation
- Measurement risks:
  - synthetic host-only tests can verify ranking logic but not GPU realism; mitigation: document that evidence in this phase is algorithmic, not performance proof
- Integration risks:
  - later kernel integration may need extra candidate constraints; mitigation: expose `grid_wave_multipliers`, `shared_mem`, and `max_threads_per_block` in the spec now

## Acceptance Criteria

- [ ] `include/cuda_test/autotune/search.hpp` provides the public autotune API
- [ ] `tune_kernel()` generates candidates from block sizes and wave multipliers and filters invalid block sizes before measurement
- [ ] winner selection follows `median -> p95 -> cv` and produces a non-empty reason
- [ ] host-only autotune unit tests pass in `msvc` and `default`-style builds
- [ ] umbrella header exports the autotune module without breaking existing host-only builds

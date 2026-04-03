# Feature Packet

## Task

- Name: Phase 4 - Benchmark / benchmark-runner
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/benchmark-runner`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §5 (functional requirement for autotuned measurements), §6 (30 measured runs and 95% CI), §11 (benchmark module), §12 (`RunStats`), §13 (autotune ranking inputs), §15 (stage metrics), §18 (performance acceptance)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 2, 4, 5; agreed stack with Google Benchmark + CUDA Runtime API

## Objective

Реализовать минимальный benchmark-слой, который можно повторно использовать в autotune и profiling-фазах. Результат фазы должен дать вычисление `RunStats` по серии sample values, `BenchmarkRunner` с warm-up и measured iterations для `ProfilingBreakdown`, а также минимальную интеграцию с Google Benchmark через smoke target в `benchmarks/`.

## Scope

- In scope:
  - `compute_stats()` для расчёта `mean`, `median`, `p95`, `ci95`, `cv`
  - `BenchmarkConfig`, `BenchmarkResult`, `BenchmarkRunner`
  - агрегация `ProfilingBreakdown` в stage-specific `RunStats`
  - host-only unit-тесты для статистики и benchmark runner
  - минимальный Google Benchmark smoke target, собираемый через `benchmarks/CMakeLists.txt`
  - подключение benchmark headers в umbrella header

- Out of scope:
  - autotune candidate generation and ranking policy execution
  - CSV/JSON reporting
  - benchmark fixtures for real course kernels
  - GPU-specific benchmark evidence under `reports/`
  - multi-stream or asynchronous orchestration

## Affected Areas

- Modules: `benchmark`
- Public headers:
  - `include/cuda_test/benchmark/stats.hpp`
  - `include/cuda_test/benchmark/benchmark_runner.hpp`
- Internal components:
  - `tests/unit/benchmark/`
  - `benchmarks/`
  - `tests/CMakeLists.txt`
  - `include/cuda_test/cuda_test.hpp`
  - `cmake/Dependencies.cmake`
- Reports or methodology:
  - implementation must stay aligned with `docs/methodology/benchmark-protocol.md`

## Interface Notes

- New or changed types:
  - `struct BenchmarkConfig`
  - `struct BenchmarkResult`
  - `class BenchmarkRunner`
- New or changed functions:
  - `core::RunStats compute_stats(const std::vector<double>& samples)`
  - `BenchmarkResult BenchmarkRunner::run(Callable&& measured_run) const`
- Input or output assumptions:
  - default policy remains `warmup_runs=5`, `measure_runs=30`
  - `compute_stats()` throws on empty input
  - `BenchmarkRunner` requires `measure_runs > 0` and a callable returning `core::ProfilingBreakdown`
  - `p95` is computed from sorted measured samples with a documented deterministic percentile rule

## Risks

- Technical risks:
  - percentile and CI formulas can drift from later reporting/autotune expectations; mitigation: lock the formulas in tests and packet now
  - template-based runner API can become too permissive; mitigation: constrain callable result to `core::ProfilingBreakdown`
- Measurement risks:
  - this phase adds infrastructure only and does not claim real kernel speedups; mitigation: keep evidence synthetic and deterministic
- Integration risks:
  - Google Benchmark linkage can vary across presets; mitigation: add a small smoke target that builds in host-only mode

## Acceptance Criteria

- [ ] `include/cuda_test/benchmark/` contains public stats and runner headers
- [ ] `compute_stats()` returns correct `mean`, `median`, `p95`, `ci95`, and `cv` for known sample sets
- [ ] `BenchmarkRunner` excludes warm-up iterations and aggregates measured `ProfilingBreakdown` values into stage stats
- [ ] host-only benchmark unit tests pass in `msvc` and `default`-style builds
- [ ] at least one Google Benchmark target builds when `CUDA_TEST_BUILD_BENCHMARKS=ON`

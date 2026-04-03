# Test Contract

## Linkage

- Related feature packet: `workitems/archive/benchmark-runner/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| `compute_stats` constant series | 30 samples, all `5.0` | `mean=median=p95=ci95_low=ci95_high=5.0`, `cv=0.0` | exact or near-equality on all fields |
| `compute_stats` known linear series | samples `1.0..30.0` | deterministic `mean`, `median`, `p95`, `ci95`, `cv` | `EXPECT_NEAR` against precomputed values |
| `compute_stats` single sample | one sample `7.5` | all summary fields collapse to `7.5`, `cv=0.0` | exact equality |
| `BenchmarkRunner` excludes warm-up | `warmup=2`, `measure=3`, deterministic counter-based callable | only iterations `3..5` are retained in `samples` and stage stats | exact sample/state checks |
| `BenchmarkRunner` aggregates stage stats | measured breakdowns with known `h2d/kernel/d2h/total` values | per-stage `RunStats` match expected mean/median values | exact or near-equality |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Empty stats input | call `compute_stats({})` | throws `std::invalid_argument` |
| Zero measured runs | `BenchmarkConfig.measure_runs = 0` | throws `std::invalid_argument` |
| Negative warm-up runs | `BenchmarkConfig.warmup_runs < 0` | throws `std::invalid_argument` |
| Measured callable failure | callable throws during warm-up or measured iteration | exception propagates to caller |

## Numerical Policy

- Tolerance strategy: exact equality for trivial series; `EXPECT_NEAR(..., 1e-9)` for derived floating-point values
- Deterministic seed strategy: no RNG; samples and runner outputs are fixed literal sequences
- Host reference implementation: expected statistics for the linear series are calculated from the locked formulas used by the phase (`median` of sorted values, linearly interpolated `p95`, sample-standard-deviation-based `ci95` and `cv`)

## Performance Scenarios

- Warm-up runs: default `5`, plus explicit test override `2`
- Measured runs: default `30`, plus explicit test override `1` and `3`
- Input sizes: sample counts `1`, `3`, `30`
- Metrics to capture: `mean_ms`, `median_ms`, `p95_ms`, `ci95_low`, `ci95_high`, `cv`; per-stage stats for `h2d`, `kernel`, `d2h`, `total`
- Baseline comparison: not applicable in this phase; smoke benchmark only validates Google Benchmark integration

## Evidence Required

- Unit or integration tests: `tests/unit/benchmark/` covers stats formulas, runner aggregation, and negative configuration cases
- Benchmark output: one Google Benchmark smoke target under `benchmarks/` builds successfully
- Report artifacts: none for this phase


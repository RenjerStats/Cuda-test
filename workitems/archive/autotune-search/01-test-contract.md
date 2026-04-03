# Test Contract

## Linkage

- Related feature packet: `workitems/archive/autotune-search/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Single candidate | `block_sizes={64}`, `grid_wave_multipliers={2}`, `problem_size=130` | one valid candidate, `grid.x=6`, winner is that candidate | exact equality |
| Lowest median wins | three candidates with distinct synthetic kernel medians | winner uses the lowest `median_ms` | exact equality |
| `p95` tie-break | two candidates with equal median and different `p95` | winner uses lower `p95_ms`; reason mentions `p95_ms` | exact equality + string contains |
| `cv` tie-break | two candidates with equal median and equal `p95` but different `cv` | winner uses lower `cv`; reason mentions `cv` | exact equality + string contains |
| Invalid block filtered | candidate list contains one block size above `max_threads_per_block` | oversized candidate is absent from `all_candidates` and is never measured | exact equality + invocation count |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Empty candidate space | `block_sizes={}` or `grid_wave_multipliers={}` | throws `std::invalid_argument` |
| Invalid run counts | `warmup_runs < 0` or `measure_runs <= 0` | throws `std::invalid_argument` |
| Invalid candidate values | non-positive block size, non-positive wave multiplier, or non-positive `max_threads_per_block` | throws `std::invalid_argument` |
| No valid candidates remain | all candidates filtered or validator rejects all | throws `std::runtime_error` |
| Measured callable failure | `measured_run(config)` throws during candidate evaluation | exception propagates |

## Numerical Policy

- Tolerance strategy: exact equality for synthetic candidate medians and rankings; `EXPECT_NEAR(..., 1e-9)` only where derived floating-point values are asserted
- Deterministic seed strategy: no RNG; candidate measurements are fixed literal sequences keyed by block/grid pairs
- Host reference implementation: expected winner is determined analytically from the locked ranking policy `median -> p95 -> cv`

## Performance Scenarios

- Warm-up runs: default `5`, plus explicit test overrides `0` and `1`
- Measured runs: default `30`, plus explicit test override `3`
- Input sizes: `130`, `128`, `256`
- Metrics to capture: winner `kernel` `RunStats`, full candidate `BenchmarkResult`, winner reason string
- Baseline comparison: not applicable in this phase; autotune only ranks the supplied candidate set

## Evidence Required

- Unit or integration tests: `tests/unit/autotune/` covers candidate generation, filtering, ranking, and invalid specs
- Benchmark output: none for this phase
- Report artifacts: none for this phase


# Test Contract

## Linkage

- Related feature packet: `workitems/active/profiling-timer/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Default breakdown | new `StagedTimer`, no recorded stages | `breakdown()` returns all zeros | exact equality |
| Timer smoke | trivial CUDA kernel, `N=256` | `h2d_ms >= 0`, `kernel_ms >= 0`, `d2h_ms >= 0`, `total_ms >= kernel_ms` | inequality checks |
| Timer reset | one measured pass, then `reset()` | `breakdown()` returns zeros after reset | exact equality |
| Timer reuse | same timer used for two passes | second pass returns valid non-negative timings without stale accumulation | inequality checks |
| Named wrappers | use `start_h2d/stop_h2d/...` wrappers | wrappers populate the same fields as generic stage API | field non-negativity and completion |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Stop without start | call `stop(Stage::kernel)` before `start(Stage::kernel)` | throws `std::logic_error` |
| Double start without stop | call `start(Stage::h2d)` twice | throws `std::logic_error` |
| Invalid CUDA runtime path | CUDA runtime returns an error during event operation | error is propagated through `CUDA_CHECK` / exception |

## Numerical Policy

- Tolerance strategy: timings are compared with inequalities only; exact values are not asserted
- Deterministic seed strategy: fixed monotonic input vectors for transfer and kernel smoke tests
- Host reference implementation: no numeric kernel reference is needed; kernel writes deterministic `out[i] = in[i] + 1`

## Performance Scenarios

- Warm-up runs: 1 warm-up pass before the consistency loop
- Measured runs: 10 timed passes for the consistency check
- Input sizes: `256`
- Metrics to capture: `kernel_ms` and `total_ms`, plus complete `ProfilingBreakdown`
- Baseline comparison: not applicable in this phase; only timer stability is checked

## Evidence Required

- Unit or integration tests: `tests/unit/profiling/` covering default state, smoke, reset/reuse, and consistency behavior
- Benchmark output: none for this phase
- Report artifacts: none for this phase

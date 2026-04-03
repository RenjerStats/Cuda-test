# Test Contract

## Linkage

- Related feature packet: `workitems/active/analysis-reporting/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| `transfer_compute_ratio` happy path | `h2d=1.0`, `kernel=2.0`, `d2h=1.0` | ratio `1.0` | exact equality |
| CSV export with candidates | `AutoTuneResult` with two candidates and one winner | CSV contains deterministic header and two data rows; winner row flagged | parse exported lines and compare fields |
| CSV export empty result | `AutoTuneResult` with `all_candidates={}` | file contains only header row | line-count and header equality |
| JSON export structured summary | same two-candidate `AutoTuneResult` | JSON contains `reason`, `winner_index`, candidate count, winner marker, and key numeric fields | field-oriented string checks |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Invalid transfer ratio input | `kernel_ms <= 0.0` | throws `std::invalid_argument` |
| Invalid export path | path points to a directory instead of a writable file | throws `std::runtime_error` |

## Numerical Policy

- Tolerance strategy: `EXPECT_DOUBLE_EQ` for the ratio happy path; string-to-double parsing in CSV tests uses `EXPECT_NEAR(..., 1e-9)` where needed
- Deterministic seed strategy: no RNG; reporting tests use fixed literal `AutoTuneResult` fixtures
- Host reference implementation: expected CSV rows and JSON key/value snippets are derived analytically from the fixed fixture result object

## Performance Scenarios

- Warm-up runs: not applicable
- Measured runs: not applicable
- Input sizes: two-candidate result fixture, empty-result fixture
- Metrics to capture: none beyond exported fields
- Baseline comparison: not applicable

## Evidence Required

- Unit or integration tests: `tests/unit/analysis/` and `tests/unit/reporting/` cover metric semantics and export contents
- Benchmark output: none for this phase
- Report artifacts: none for this phase

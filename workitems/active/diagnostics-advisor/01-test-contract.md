# Test Contract

## Linkage

- Related feature packet: `workitems/active/diagnostics-advisor/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Healthy fingerprint | `tcr=0.5, occ=0.8, bw=0.3, cv=0.05, block=1.1, scaling=1.0, regs=16, lmem=0` | empty recommendation list | size check |
| `transfer_dominated` | `tcr=5.0`, others healthy | one recommendation with `tag="transfer_dominated"` and `severity="critical"` | tag/severity match |
| `low_occupancy` | `occ=0.3, regs=48`, others healthy | one recommendation with `tag="low_occupancy"` | tag match |
| `low_occupancy` guard | `occ=0.3, regs=16`, others healthy | no `low_occupancy` recommendation | absence check |
| `local_memory_pressure` | `local_size_bytes=32`, others healthy | one recommendation with `tag="local_memory_pressure"` | tag match |
| `local_memory_pressure` guard | `local_size_bytes=0`, others healthy | no `local_memory_pressure` recommendation | absence check |
| `bandwidth_bound` | `bw=0.9`, others healthy | one recommendation with `tag="bandwidth_bound"` | tag match |
| `unstable_timing` | `cv=0.25`, others healthy | one recommendation with `tag="unstable_timing"` and `severity="warning"` | tag/severity match |
| `block_sensitive` | `block=2.0`, others healthy | one recommendation with `tag="block_sensitive"` | tag match |
| `superlinear_scaling` | `scaling=1.5`, others healthy | one recommendation with `tag="superlinear_scaling"` | tag match |
| `superlinear_scaling` guard | `scaling=0.0`, others healthy | no `superlinear_scaling` recommendation | absence check |
| `well_utilized` | `tcr=0.1, occ=0.7, cv=0.05, block=1.1`, no other issue | one recommendation with `tag="well_utilized"` | tag match |
| `well_utilized` guard | `tcr=0.1, occ=0.7, cv=0.05, block=1.1`, plus `local_size_bytes=32` | no `well_utilized` recommendation | absence check |
| Multiple findings | `tcr=5.0, cv=0.3, block=2.0` | recommendations include `transfer_dominated`, `unstable_timing`, `block_sensitive` | size + tag set |
| Recommendation fields | any triggered recommendation | `tag`, `severity`, `summary`, `suggestion` are all non-empty | string length check |
| Boundary `tcr == 3.0` | exact threshold | no `transfer_dominated` recommendation | absence check |
| Boundary `tcr == 3.01` | just above threshold | `transfer_dominated` recommendation fires | presence check |
| Pipeline diagnose with benchmark only (CUDA) | `pipeline(desc).benchmark(cfg).diagnose().run()` | `fingerprint` has value, `transfer_compute_ratio >= 0`, `cv >= 0`, `block_sensitivity == 0` | field checks |
| Pipeline diagnose with autotune only (CUDA) | `pipeline(desc).autotune(spec).diagnose().run()` | `fingerprint` has value, `block_sensitivity > 0`, `recommendations()` accessible | field checks |
| Pipeline diagnose with benchmark + autotune (CUDA) | `pipeline(desc).benchmark(cfg).autotune(spec).diagnose().run()` | `fingerprint` has value and `recommendations()` accessible | field checks |
| Pipeline diagnose without timing stages | `pipeline(desc).diagnose().run()` | no crash, no fingerprint, empty recommendations | graceful handling |
| Pipeline JSON export with diagnostics (CUDA) | report from `.diagnose()` exported to JSON | JSON contains `fingerprint` object and `recommendations` array | string/field checks |
| Pipeline CSV export with diagnostics (CUDA) | same report exported to CSV | header contains diagnostics columns and row contains recommendation summary fields | string/field checks |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| All-zero fingerprint | all fields set to `0` | empty recommendation list |
| Negative metrics | `tcr=-1.0, cv=-0.1, bw=-0.5` | no crash; rules do not fire from invalid-negative values |
| Pipeline diagnose without CUDA device | integration test machine has no visible GPU | CUDA diagnose test is skipped cleanly |

## Numerical Policy

- Boundary checks use exact threshold values and near-threshold probes (`+0.01`)
- Synthetic fingerprints are deterministic and hand-authored
- No stochastic acceptance threshold beyond the benchmark/autotune statistics already produced by the library

## Performance Scenarios

- Not applicable: rule evaluation is O(number of rules) and negligible relative to benchmarking

## Evidence Required

- Host-only unit tests:
  - `tests/unit/analysis/advisor_test.cpp`
- Reporting tests:
  - `tests/unit/reporting/export_test.cpp`
- CUDA integration tests:
  - `tests/integration/pipeline/pipeline_diagnose_test.cu`
- Validation commands:
  - host-only preset for unit and reporting tests
  - CUDA preset for pipeline diagnose integration

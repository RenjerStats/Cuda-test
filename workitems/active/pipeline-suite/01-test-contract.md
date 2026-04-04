# Test Contract

## Linkage

- Related feature packet: `workitems/active/pipeline-suite/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Pipeline — no stages | valid descriptor, no `.correctness()/.benchmark()/.autotune()` | `passed()==true`, all stage flags false, all optional results empty | field assertions |
| Pipeline — correctness only (CUDA) | valid vector-add descriptor | `passed()==true`, `correctness_enabled()==true`, benchmark/autotune results empty | field assertions |
| Pipeline — correctness fails (CUDA) | descriptor with deliberately wrong expected | `passed()==false`, benchmark/autotune results empty, later stages not executed | field assertions |
| Pipeline — benchmark only (CUDA) | descriptor with working `measure()` | `benchmark_result().has_value()==true`, sampled stats are positive, correctness flag false | field assertions + stats positivity |
| Pipeline — autotune only (CUDA) | descriptor with valid `measure()` and `validate()` | `autotune_result().has_value()==true`, candidate count matches config space | field assertions |
| Pipeline — full chain (CUDA) | correctness + benchmark + autotune enabled | report contains correctness success, benchmark result, autotune result | comprehensive field check |
| Pipeline — device selection (CUDA) | `.device(selected_device_id())` | `report.device_id()==selected_device_id()`, benchmark/autotune configs inherit that device | field assertions + candidate assertions |
| Pipeline — benchmark baseline config (CUDA) | problem size 1000, default baseline | `benchmark` uses `block.x==128`, `grid.x==8` | baseline config assertions |
| Pipeline — autotune config propagation (CUDA) | `AutoTuneSpec{block_sizes={64,128}, grid_wave_multipliers={1,2}}` + `.device(selected_device_id())` | autotune candidates use selected device, candidate count is 4 when all valid | candidate assertions |
| PipelineReport::to_json | report with correctness + benchmark + autotune | file exists, contains kernel name and stage fields, JSON-shaped text | file existence + substring assertions |
| PipelineReport::to_csv | report with correctness + benchmark + autotune | file exists, header row present, one data row present | file existence + line count |
| Suite — empty | no descriptors | `reports().empty()==true`, `all_passed()==true` | field assertions |
| Suite — common stage profile | 2 descriptors, suite configured with correctness + benchmark | both reports execute the same stages | report field assertions |
| Suite — duplicate name rejection | add two descriptors with same `name()` | second `add()` throws | exception assertion |
| Suite — partial failure | two descriptors, one fails correctness | `all_passed()==false`; failed report has no benchmark/autotune results | per-report assertions |
| SuiteReport::to_json(dir) | 2 descriptors | directory gets 2 JSON files named `<suite>_<kernel>.json` | file existence per kernel |
| SuiteReport::to_csv(dir) | 2 descriptors | directory gets 2 CSV files named `<suite>_<kernel>.csv` | file existence per kernel |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Pipeline with empty autotune block list | `.autotune({})` or spec with `block_sizes.empty()` | throws `std::invalid_argument` at configuration time or on `.run()` |
| Pipeline with invalid benchmark config | `BenchmarkConfig{warmup_runs=-1}` or `measure_runs=0` | exception from `BenchmarkRunner` propagates |
| Suite export to nested missing directory | output directory does not exist | parent directories are created automatically |
| Suite with empty name | `suite("").add(...)` | allowed; file names derive from kernel names without suite prefix collisions handled by duplicate-name rule |

## Numerical Policy

- Tolerance strategy: correctness behavior is exercised through `KernelDescriptor`; pipeline tests validate boolean/report semantics rather than element-wise comparisons
- Deterministic seed strategy: fixed monotonic host vectors for CUDA tests; host-only tests use deterministic temp-file/report fixtures
- Host reference implementation: generated before descriptor construction, as in `kernel-descriptor`

## Performance Scenarios

- Warm-up runs:
  - host-only orchestration tests use small configs like `{warmup_runs=1, measure_runs=2}`
  - CUDA integration tests use reduced configs like `{warmup_runs=1, measure_runs=3}` for runtime control
- Input sizes:
  - host-only mock tests: `problem_size=1000`
  - CUDA integration tests: `1024` and `65536`
- Metrics to capture:
  - positivity and presence of `BenchmarkResult` / `AutoTuneResult`
  - candidate count and device/config propagation
  - no assertions on exact timing numbers
- Baseline comparison:
  - benchmark stage is validated against `KernelDescriptor::baseline_config()`

## Evidence Required

- Host-only unit tests:
  - `tests/unit/pipeline/pipeline_test.cpp`
  - covers no-stage pipeline, report export, suite duplicate names, empty suite, suite file emission
- CUDA integration tests:
  - `tests/integration/pipeline/pipeline_cuda_test.cu`
  - covers correctness-only, failure short-circuit, benchmark-only, autotune-only, full chain, device propagation, suite over multiple descriptors and filesystem export
- Benchmark output: none beyond report objects validated inside tests
- Report artifacts: JSON/CSV files created under temp directories, verified, then cleaned up by test setup/teardown

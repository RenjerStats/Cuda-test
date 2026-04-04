# Test Contract

## Linkage

- Related feature packet: `workitems/active/export-refactor/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| JSON — BenchmarkResult | 5 sample breakdowns with known values | JSON contains `sample_count:5`, all 4 stage stats objects with correct field names | JSON parse + field existence + value check |
| CSV — BenchmarkResult | 5 sample breakdowns | CSV has header row with 24 columns (6 stats × 4 stages), 1 data row | line count + column count |
| JSON — BenchmarkResult — stats correctness | samples: [1.0, 2.0, 3.0, 4.0, 5.0] for kernel_ms | `kernel_stats.mean_ms == 3.0`, `kernel_stats.median_ms == 3.0` | exact value match |
| JSON — PipelineReport — full | correctness=true, bench present, tune present | JSON has all 3 sections: `correctness`, `benchmark`, `autotune` | JSON parse + section existence |
| JSON — PipelineReport — correctness only | correctness=true, bench=nullopt, tune=nullopt | JSON has `correctness` section, no `benchmark` or `autotune` keys | JSON parse + key absence |
| JSON — PipelineReport — benchmark only | correctness not enabled, bench present, tune=nullopt | JSON has `benchmark`, `correctness.enabled==false`, no `autotune` | JSON parse + conditional check |
| CSV — PipelineReport — full | all stages present | CSV header includes kernel_name, correctness, bench fields, tune fields; 1 data row | column count + data presence |
| CSV — PipelineReport — partial | only benchmark, no autotune | tune columns present in header but empty in data row | column count + empty value check |
| PipelineReport member delegation | valid report with bench+tune | `report.to_json()` and `reporting::export_json()` produce identical output; same for CSV | file text equality |
| Backward compat — AutoTuneResult JSON | same input as existing test | identical output format to current implementation | compare field names and structure |
| Backward compat — AutoTuneResult CSV | same input as existing test | identical output format to current implementation | compare header columns |
| Parent directory creation | export to `tmp/subdir/file.json` where `tmp/subdir` doesn't exist | file created, parent dirs exist | file existence |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Empty BenchmarkResult | `BenchmarkResult` with 0 samples | JSON contains `sample_count:0`, stats are all zeros | field check |
| PipelineReport with failed correctness | `correctness_passed=false` | JSON `correctness.passed==false`; bench/tune sections absent (early abort) | JSON parse |
| Invalid path | path to read-only location or invalid characters | throws `std::runtime_error` or filesystem error | exception assertion |

## Numerical Policy

- Tolerance strategy: exact string/value matching for JSON fields; CSV columns checked by name not position where possible
- Deterministic seed strategy: hand-crafted `ProfilingBreakdown` samples with known values (e.g., h2d=1.0, kernel=2.0, d2h=0.5, total=3.5)
- Host reference implementation: expected JSON/CSV structure manually specified in test

## Performance Scenarios

- Not applicable — export is an I/O operation, not benchmarked

## Evidence Required

- Unit tests: `tests/unit/reporting/export_test.cpp` — BenchmarkResult, no-stage/partial PipelineReport, delegation, and AutoTune backward compatibility
- Integration tests: `tests/integration/pipeline/pipeline_cuda_test.cu` — full PipelineReport envelope (`benchmark` + `autotune`) and failed-correctness export shape
- Benchmark output: none
- Report artifacts: none (test files are temporary)

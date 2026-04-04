# Implementation Notes

## Summary

Implemented `export-refactor` by making `reporting` the single formatting authority for `BenchmarkResult`, `AutoTuneResult`, and `PipelineReport`.

The main changes:
- added `reporting::export_csv/json` overloads for `benchmark::BenchmarkResult`
- added `reporting::export_csv/json` overloads for `pipeline::PipelineReport`
- refactored `PipelineReport::to_csv()` / `to_json()` into thin delegating wrappers over `reporting::export_*`
- preserved the existing `AutoTuneResult` export format and tests
- updated pipeline tests to validate the new JSON envelope with `correctness`, `benchmark`, and `autotune` sections

## Files And Modules

- Files touched:
  - `include/cuda_test/pipeline/pipeline_report.hpp`
  - `include/cuda_test/reporting/export.hpp`
  - `tests/unit/reporting/export_test.cpp`
  - `tests/unit/pipeline/pipeline_test.cpp`
  - `tests/integration/pipeline/pipeline_cuda_test.cu`
  - `workitems/active/export-refactor/00-feature-packet.md`
  - `workitems/active/export-refactor/01-test-contract.md`
  - `workitems/active/export-refactor/02-implementation-notes.md`
- Modules touched:
  - `reporting` (expanded export surface)
  - `pipeline` (delegation only; no longer owns formatting logic)

## Deviations From Packet

- `PipelineReport` JSON export now uses a structured envelope with:
  - top-level `kernel_name`, `device_id`, `passed`, `benchmark_enabled`, `autotune_enabled`
  - nested `correctness`
  - conditional `benchmark` / `autotune` sections only when data is present
  This is slightly richer than the draft packet and keeps enough stage-status context for failed correctness runs.
- `BenchmarkResult` export does not include launch config.
  Reason: `benchmark::BenchmarkResult` does not carry config information in the current public type.
- Full `PipelineReport` export validation is split across:
  - host unit tests for no-stage / partial cases and delegation
  - existing CUDA integration tests for full bench+tune cases
  Reason: constructing a non-trivial `PipelineReport` with benchmark/autotune data without CUDA would require widening the public construction API just for tests.

## Validation

- Build commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build --preset msvc --target reporting_export_test pipeline_suite_test --config Debug`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build --preset msvc-cuda --target pipeline_suite_cuda_test --config Debug`
- Test commands run:
  - `build/msvc/tests/Debug/reporting_export_test.exe`
  - `build/msvc/tests/Debug/pipeline_suite_test.exe`
  - `build/msvc-cuda/tests/Debug/pipeline_suite_cuda_test.exe`
- Result summary:
  - `reporting_export_test.exe`: 10/10 passed
  - `pipeline_suite_test.exe`: 8/8 passed
  - `pipeline_suite_cuda_test.exe`: 7/7 passed

## Follow-Ups

- `html-export` can now consume the unified `reporting` surface instead of special-casing `PipelineReport`
- `diagnostics-advisor` can extend the `PipelineReport` JSON envelope by adding a new conditional section without reintroducing formatting logic into `pipeline_report.hpp`

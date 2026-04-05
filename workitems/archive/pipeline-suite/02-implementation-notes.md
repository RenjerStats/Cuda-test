# Implementation Notes

## Summary

Implemented the second post-MVP task, `pipeline-suite`, as a header-only orchestration layer on top of the accepted `KernelDescriptor`.

The new API covers the revised packet:
- `Pipeline` executes `correctness`, `benchmark`, and `autotune` in one `.run()`
- `Suite` applies a shared pipeline profile to multiple kernel descriptors via `.run_all()`
- `PipelineReport` and `SuiteReport` provide programmatic results plus JSON/CSV export
- root convenience forwarders `make_pipeline(...)` and `make_suite(...)` are available alongside the canonical `cuda_test::pipeline::...` API
- host-only unit tests cover no-stage semantics, negative config paths, duplicate-name protection, empty suite, and file export
- CUDA integration tests cover correctness-only, failure short-circuit, benchmark-only, autotune-only, full chain, suite success, and suite partial failure
- post-review cleanup added the two missing negative tests from the contract and replaced positional `BenchmarkConfig{...}` literals in CUDA tests with explicit config builders

## Files And Modules

- Files touched:
  - `include/cuda_test/pipeline/pipeline_report.hpp`
  - `include/cuda_test/pipeline/pipeline.hpp`
  - `include/cuda_test/pipeline/suite.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `tests/CMakeLists.txt`
  - `tests/unit/pipeline/pipeline_test.cpp`
  - `tests/integration/pipeline/pipeline_cuda_test.cu`
  - `workitems/active/pipeline-suite/00-feature-packet.md`
  - `workitems/active/pipeline-suite/01-test-contract.md`
  - `workitems/active/pipeline-suite/02-implementation-notes.md`
- Modules touched:
  - `pipeline` (expanded from descriptor-only to orchestration layer)
  - umbrella include surface in `cuda_test.hpp`
  - test registration in `tests/`

## Deviations From Packet

- Replaced the draft entry points `pipeline(...)` / `suite(...)` with `make_pipeline(...)` / `make_suite(...)`.
  Reason: `cuda_test::pipeline` is already a namespace, so a root helper named `pipeline(...)` would conflict with the namespace name.
- Kept the implementation header-only and did not add `src/pipeline/`.
  Reason: the current library architecture is header-only, and the new layer fits that model cleanly.
- Strengthened `Suite::add()` beyond plain duplicate-name checks: it now also rejects collisions after filename sanitization.
  Reason: two different raw names like `alpha beta` and `alpha/beta` would otherwise overwrite the same exported file stem.
- `PipelineReport::to_json()` / `to_csv()` are implemented inside `pipeline_report.hpp` instead of delegating to `reporting::export_*`.
  Reason: the current `reporting` module only exports `AutoTuneResult`; the cross-module export unification remains deferred to `export-refactor`.

## Validation

- Configure commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc-cuda`
- Build commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build --preset msvc --target pipeline_suite_test --config Debug`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build --preset msvc-cuda --target pipeline_suite_cuda_test --config Debug`
- Test commands run:
  - `build/msvc/tests/Debug/pipeline_suite_test.exe`
  - `build/msvc-cuda/tests/Debug/pipeline_suite_cuda_test.exe`
- Result summary:
  - host-only unit binary passed: 8/8 tests
  - CUDA integration binary passed: 7/7 tests
  - CUDA build emitted the same NVCC warning family `#177-D` from generated Google Test registration code; it did not affect correctness or binary production

## Follow-Ups

- Pending task dependencies:
  - `export-refactor` should unify `PipelineReport`/`SuiteReport` export with the `reporting` module
  - `diagnostics-advisor` will extend the pipeline chain with `.diagnose()`
- Known inherited cost:
  - `Pipeline` and `Suite` still pay repeated `KernelDescriptor` allocation/factory overhead during benchmark/autotune because descriptor-level caching is not implemented yet

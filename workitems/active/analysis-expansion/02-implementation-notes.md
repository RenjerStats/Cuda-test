# Implementation Notes

## Summary

Expanded the `analysis` module from a single derived metric into a header-only runtime/derived-metrics layer. The implementation now covers:

- occupancy estimation and block-size suggestion;
- kernel function attribute queries;
- effective/theoretical bandwidth estimation;
- device memory headroom via `cudaMemGetInfo`;
- derived metrics `block_sensitivity` and `scaling_exponent`;
- `KernelFingerprint` aggregation for later diagnostics work.

Before implementation, the packet and test contract were tightened against official CUDA Runtime API behavior. The main change was making occupancy block-size suggestion a template wrapper instead of a `const void*` API, and documenting `cudaMemGetInfo` as a headroom estimate rather than a precise bottleneck metric.

## Files And Modules

- Files touched:
  - `include/cuda_test/analysis/detail/runtime_api.hpp`
  - `include/cuda_test/analysis/occupancy.hpp`
  - `include/cuda_test/analysis/kernel_attributes.hpp`
  - `include/cuda_test/analysis/bandwidth.hpp`
  - `include/cuda_test/analysis/memory_info.hpp`
  - `include/cuda_test/analysis/fingerprint.hpp`
  - `include/cuda_test/analysis/metrics.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `tests/CMakeLists.txt`
  - `tests/unit/analysis/bandwidth_test.cpp`
  - `tests/unit/analysis/metrics_extended_test.cpp`
  - `tests/integration/analysis/analysis_cuda_runtime_test.cu`
  - `workitems/active/analysis-expansion/00-feature-packet.md`
  - `workitems/active/analysis-expansion/01-test-contract.md`
  - `workitems/active/post-mvp-vision/00-vision.md`
- Modules touched:
  - `analysis`
  - `tests`

## Deviations From Packet

- `suggest_block_size` was implemented as a template wrapper over the Runtime C++ occupancy helper instead of a `const void*` function. This matches the actual Runtime API surface and was back-propagated into the packet/test contract before code changes.
- The theoretical-bandwidth implementation uses `cudaDeviceGetAttribute` (`cudaDevAttrMemoryClockRate`, `cudaDevAttrGlobalMemoryBusWidth`) instead of reading `cudaDeviceProp` fields directly. This was required for compatibility with the installed CUDA 13.0 headers/toolchain.
- `KernelFingerprint::build_fingerprint(...)` takes `core::ProfilingBreakdown` + `core::RunStats` + optional metric structs instead of depending directly on `BenchmarkResult`/`AutoTuneResult`. This keeps the analysis layer decoupled from orchestration/reporting and still exposes all fields needed by the next diagnostics phase.
- `coalescing_sensitivity` from §15 of the explanatory note was intentionally deferred. Unlike the Runtime-API-backed metrics in this task, it requires a purpose-built benchmark scenario comparing linear vs. strided memory access patterns, so it was documented as out of scope for this phase instead of being implemented half-way.

## Validation

- Commands run:
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset msvc`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset msvc --target analysis_metrics_test analysis_bandwidth_test analysis_metrics_extended_test`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc\tests\Debug\analysis_metrics_test.exe'`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc\tests\Debug\analysis_bandwidth_test.exe'`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc\tests\Debug\analysis_metrics_extended_test.exe'`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset msvc-cuda`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset msvc-cuda --target analysis_metrics_test analysis_bandwidth_test analysis_metrics_extended_test analysis_cuda_runtime_test`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc-cuda\tests\Debug\analysis_metrics_test.exe'`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc-cuda\tests\Debug\analysis_bandwidth_test.exe'`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc-cuda\tests\Debug\analysis_metrics_extended_test.exe'`
  - `& 'E:\source\EducationPolitech\year_2\Cuda test\build\msvc-cuda\tests\Debug\analysis_cuda_runtime_test.exe'`
- Result summary:
  - Host-only build: `analysis_metrics_test` passed, `analysis_metrics_extended_test` passed, `analysis_bandwidth_test` passed with the CUDA-dependent cases skipped because the host-only preset disables CUDA support.
  - CUDA build: all four analysis test executables passed.

## Follow-Ups

- Pending cleanup:
  - The CUDA test target emits NVCC warning `#177-D` from the generated Google Test registration glue (`test_info_ was declared but never referenced`). This is noise from the toolchain/GTest interaction, not from the analysis code itself.
- Deferred work:
  - Integrate these metrics into `Pipeline`/`Suite` through the planned `diagnostics-advisor` task.
  - Add `coalescing_sensitivity` through a dedicated benchmark methodology task or as part of diagnostics once the project has explicit strided-vs-linear experiment fixtures.
  - Decide whether `KernelDescriptor` should eventually capture an optional raw kernel symbol for automatic occupancy/attribute collection inside Layer 3.

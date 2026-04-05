# Implementation Notes

## Summary

Implemented a header-only diagnostics layer in `analysis` and integrated it into
`Pipeline` through `.diagnose()`.

The new `analysis::diagnose(const KernelFingerprint&)` turns the runtime-derived
metrics from `analysis-expansion` into concrete recommendations. The rule set was
tightened against official NVIDIA guidance before coding:

- transfer advice is based on minimizing host/device traffic, pinned memory, and
  overlapping copies with compute;
- occupancy advice is framed as a latency-hiding signal, not as a direct
  performance score;
- bandwidth advice is framed around coalescing, redundant global-memory traffic,
  shared memory, and bank conflicts;
- timing-instability advice is framed around competing CUDA contexts/processes and
  measurement noise.

Pipeline diagnostics intentionally use only the metrics the current pipeline layer
can compute honestly: transfer/compute ratio, timing CV, and block sensitivity.

## Files And Modules

- Files touched:
  - `include/cuda_test/analysis/advisor.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `include/cuda_test/pipeline/pipeline.hpp`
  - `include/cuda_test/pipeline/pipeline_report.hpp`
  - `include/cuda_test/reporting/export.hpp`
  - `tests/CMakeLists.txt`
  - `tests/unit/analysis/advisor_test.cpp`
  - `tests/unit/pipeline/pipeline_test.cpp`
  - `tests/unit/reporting/export_test.cpp`
  - `tests/integration/pipeline/pipeline_diagnose_test.cu`
  - `workitems/active/diagnostics-advisor/00-feature-packet.md`
  - `workitems/active/diagnostics-advisor/01-test-contract.md`
- Modules touched:
  - `analysis`
  - `pipeline`
  - `reporting`
  - `tests`

## Deviations From Packet

- Added `local_memory_pressure` as an explicit rule because `KernelFingerprint`
  already carried `local_size_bytes`, and NVIDIA documents local memory as
  off-chip and commonly caused by register pressure or oversized locals. This made
  the advice set materially more useful than leaving `local_size_bytes` unused.
- `well_utilized` was deliberately weakened relative to the original vision text.
  After reviewing NVIDIA occupancy guidance, it now fires only when no other rule
  fires and the available metrics look quiet/balanced. This avoids over-claiming
  that high occupancy alone means the kernel is "good".
- `Pipeline` diagnostics use median stage timings from the benchmark result when
  available, otherwise the winning autotune candidate. This was chosen so the
  pipeline can produce a useful `KernelFingerprint` without inventing new
  benchmarking stages.
- `Pipeline` still does not populate occupancy, register count, local memory,
  bandwidth utilization, or scaling exponent. Those require data that the current
  descriptor/pipeline layer does not own yet, so the implementation leaves them at
  default values rather than guessing.

## External Research Applied

- NVIDIA CUDA C++ Best Practices Guide 12.6.1:
  - 8.2 Bandwidth
  - 9.1 Data Transfer Between Host and Device
  - 9.2 Device Memory Spaces
  - 10.1 Occupancy
  - 10.3 Thread and Block Heuristics
  - 10.6 Multiple contexts
- NVIDIA CUDA Runtime API:
  - Occupancy API
  - `cudaFuncAttributes`

These references were used to revise the packet before implementation. Where the
code makes an inference rather than encoding a direct NVIDIA threshold, the text is
kept broad and framed as a likely next debugging direction rather than a guarantee.

## Validation

- Commands run:
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset msvc`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset msvc --config Debug --target analysis_advisor_test pipeline_suite_test reporting_export_test`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' -C Debug --output-on-failure -R '^(AdvisorTest\.|PipelineSuiteTest\.|ExportTest\.)'`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset msvc --config Debug --target cuda_test_tests`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset msvc-cuda`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset msvc-cuda --config Debug --target pipeline_diagnose_cuda_test analysis_advisor_test reporting_export_test`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' -C Debug --output-on-failure -R '^(AdvisorTest\.|PipelineDiagnoseCudaTest\.)'`
  - `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset msvc-cuda --config Debug --target cuda_test_tests`
- Result summary:
  - Host-only build: all new advisor, pipeline, and reporting tests passed.
  - CUDA build: all advisor tests passed, and all three new `PipelineDiagnoseCudaTest.*`
    integration tests passed.
  - Both `msvc` and `msvc-cuda` successfully built the full `cuda_test_tests` umbrella
    target after the diagnostics changes.

## Follow-Ups

- The pipeline still needs raw kernel symbol / transfer-byte plumbing if future
  tasks want occupancy-, register-, or bandwidth-driven advice directly inside
  `.diagnose()`.
- The NVCC `#177-D` warnings from generated GoogleTest registration code remain
  toolchain noise and are unchanged by this task.

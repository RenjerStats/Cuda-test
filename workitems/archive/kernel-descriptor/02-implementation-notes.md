# Implementation Notes

## Summary

Implemented the first post-MVP task, `kernel-descriptor`, as a new header-only `pipeline` entry point centered around `KernelDescriptor` and `KernelDescriptorBuilder`.

The new API covers the scope from the packet:
- declarative kernel description through `describe_kernel(...).problem_size(...).inputs(...).expected(...).tolerance(...).launch(...).build()`
- type-erased runtime object `KernelDescriptor` with `name()`, `problem_size()`, `baseline_config()`, `validate()`, and `measure()`
- automatic H2D / kernel / D2H lifecycle inside the descriptor
- host-only unit tests for builder behavior and metadata
- CUDA integration tests for correctness, timing breakdowns, invalid launch propagation, zero-size handling, and descriptor reuse
- post-review cleanup: documented non-thread-safe shared use, moved `tie_device_buffers()` out of the kernel timing section, converted the tolerance type gate to `if constexpr`, and added a CUDA move-semantics functional test

## Files And Modules

- Files touched:
  - `include/cuda_test/pipeline/kernel_descriptor.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `tests/CMakeLists.txt`
  - `tests/unit/pipeline/kernel_descriptor_test.cpp`
  - `tests/integration/pipeline/kernel_descriptor_cuda_test.cu`
  - `workitems/active/kernel-descriptor/02-implementation-notes.md`
- Modules touched:
  - `pipeline` (new public module)
  - umbrella include surface in `cuda_test.hpp`
  - test registration in `tests/`

## Deviations From Packet

- No `src/pipeline/` source files were added. The implementation is header-only to stay aligned with the current library architecture, which is also header-only.
- Added a small convenience forwarder `cuda_test::describe_kernel(...)` and `using pipeline::KernelDescriptor` in the root namespace. This is non-breaking and keeps the API closer to the post-MVP vision examples while preserving the canonical module path `cuda_test::pipeline`.
- For runtime safety, `KernelDescriptor` performs `CUDA_CHECK(cudaGetLastError())` after the launch callable returns. The packet said the user callable may do this manually; the library now enforces it as well.
- After Claude review, documented that shared `KernelDescriptor` instances are not thread-safe for concurrent `validate()` / `measure()` calls. Buffer/factory caching remains deferred to `pipeline-suite`.

## Validation

- Commands run:
  - `cmake --preset msvc`
  - `cmake --preset msvc-cuda`
  - `cmake --build --preset msvc --target pipeline_kernel_descriptor_test --config Debug`
  - `cmake --build --preset msvc-cuda --target pipeline_kernel_descriptor_cuda_test --config Debug`
  - `build/msvc/tests/Debug/pipeline_kernel_descriptor_test.exe`
  - `build/msvc-cuda/tests/Debug/pipeline_kernel_descriptor_cuda_test.exe`
- Result summary:
  - host-only unit binary built and ran successfully: 5 passed, 1 skipped
  - CUDA integration binary built and ran successfully: 8 passed
  - `ctest` under the Visual Studio generator did not discover tests in this environment, so validation was executed directly through the produced GTest binaries
  - CUDA build emitted NVCC warning `#177-D` from generated Google Test registration code; this did not affect correctness or binary production

## Follow-Ups

- Pending cleanup:
  - Claude Opus review and `03-review-report.md`
- Deferred work:
  - `Pipeline` / `Suite` orchestration in the next task
  - report export integration for pipeline objects in `export-refactor`

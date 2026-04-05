# Getting Started

This guide shows how to install, build, and run `cuda_test` as a consumer-facing library.
It is written as an independent overview, not as an internal project note.

## What You Get

With `cuda_test`, a CUDA developer can turn one kernel into a repeatable workflow:

1. define inputs and expected outputs once
2. validate correctness
3. benchmark with warm-up and repeated measured runs
4. autotune launch parameters
5. generate high-level diagnostics
6. export HTML, JSON, and CSV artifacts

The core idea is simple: stop rebuilding one-off kernel harnesses for every experiment.

## Installation Models

## Installed package

Use this when the library is already installed in a CMake-visible prefix.

```cmake
find_package(cuda_test 0.2.0 REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

## Directly from GitHub with `FetchContent`

This is the most convenient model once the repository is published remotely.

```cmake
include(FetchContent)

set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  cuda_test
  GIT_REPOSITORY https://github.com/<org>/cuda_test.git
  GIT_TAG v0.2.0
)
FetchContent_MakeAvailable(cuda_test)

target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

## Vendored source with `add_subdirectory`

```cmake
set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/cuda_test)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

If you also want the optional `cuda_test::testing` target, see
[docs/integration.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/integration.md).

## Building This Repository

On the current Windows setup the repository already defines CMake presets.

### Host-only preset

```powershell
cmake --preset msvc
cmake --build --preset msvc --config Debug
```

### CUDA-enabled preset

```powershell
cmake --preset msvc-cuda
cmake --build --preset msvc-cuda --config Debug
```

Use the CUDA preset when you want real kernel execution, examples, integration tests, and reports.

## First End-to-End Demo

The fastest way to understand the library is to run the bundled walkthrough example:

- source: [examples/full_workflow_demo.cu](/E:/source/EducationPolitech/year_2/Cuda%20test/examples/full_workflow_demo.cu)

Build just the demo:

```powershell
cmake --build --preset msvc-cuda --config Debug --target full_workflow_demo
```

Run it:

```powershell
.\build\msvc-cuda\examples\Debug\full_workflow_demo.exe
```

The demo creates output in:

- [reports/demo/full-workflow](/E:/source/EducationPolitech/year_2/Cuda%20test/reports/demo/full-workflow)

That directory contains:

- `pipeline/pipeline_report.html`: visual single-kernel report
- `pipeline/pipeline_report.json`: machine-readable pipeline envelope
- `pipeline/pipeline_report.csv`: compact flat summary
- `pipeline/benchmark.json` and `pipeline/benchmark.csv`: raw timing view
- `pipeline/autotune.json` and `pipeline/autotune.csv`: candidate sweep details
- `suite/suite_report.html`: multi-kernel visual summary
- `suite/json/` and `suite/csv/`: one export per suite entry

## What The Main API Objects Mean

## `KernelDescriptor`

This object describes one kernel experiment.

It answers:

- what inputs should be allocated and copied
- what correct output looks like
- what numeric tolerance is allowed
- how to launch the kernel for an arbitrary grid/block/shared-memory configuration

This is the stable unit reused by everything else.

## `Pipeline`

This is the single-kernel deep-dive workflow.

Use it when you want the full engineering loop for one descriptor:

- `correctness()` to reject wrong kernels before performance analysis
- `benchmark()` to measure a baseline configuration
- `autotune()` to compare launch candidates
- `diagnose()` to derive simple advice from the resulting metrics

The output is a `PipelineReport`.

## `Suite`

This is the batch workflow.

Use it when you want to run multiple descriptors under the same policy, for example:

- one kernel under several problem sizes
- one kernel with several parameter sets
- several closely related kernels in the same subsystem

The output is a `SuiteReport`.

## `reporting`

Exports are split by audience:

- `CSV` for spreadsheets and quick diffing
- `JSON` for scripts and downstream tooling
- `HTML` for human review, screenshots, and result sharing

## Minimal Consumer Example

```cpp
#include "cuda_test/cuda_test.hpp"

auto descriptor = cuda_test::describe_kernel("vector_add")
    .problem_size(1 << 20)
    .inputs([] {
        return std::make_tuple(make_a(), make_b());
    })
    .expected([] {
        return make_expected_sum();
    })
    .tolerance(1e-5)
    .launch([](const cuda_test::core::KernelLaunchConfig& config,
               auto& device_inputs,
               cuda_test::core::DeviceMemory<float>& output_device) {
        auto& a = std::get<0>(device_inputs);
        auto& b = std::get<1>(device_inputs);
        vector_add_kernel<<<config.grid, config.block, config.shared_mem>>>(
            a.data(), b.data(), output_device.data(), 1 << 20);
    })
    .build();

auto report = cuda_test::make_pipeline(descriptor)
    .device(0)
    .correctness()
    .benchmark()
    .autotune({64, 128, 256}, {1, 2})
    .diagnose()
    .run();

report.to_html("vector_add_report.html");
```

## What Diagnostics Actually Mean

The diagnostics layer is intentionally high-level.
It does not attempt to replace a low-level profiler.

It is useful for fast iteration questions like:

- are transfers dominating everything
- does the kernel look block-size sensitive
- is timing noisy enough to distrust the comparison
- is there evidence of local-memory pressure

Once a report shows a real bottleneck, the next tool is typically Nsight Compute or Nsight Systems.

## Recommended Adoption Path

If you are evaluating the library for a real project, the pragmatic path is:

1. start with one small kernel and only `correctness()`
2. add `benchmark()` once the outputs are trustworthy
3. add `autotune()` when launch choices matter
4. add `diagnose()` when you want faster iteration hints
5. switch to `Suite` when one kernel workflow is already working well

That keeps the integration cost low and avoids building too much harness upfront.

## Related Documentation

- [README.md](/E:/source/EducationPolitech/year_2/Cuda%20test/README.md)
- [docs/integration.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/integration.md)
- [docs/architecture/module-map.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/architecture/module-map.md)
- [examples/full_workflow_demo.cu](/E:/source/EducationPolitech/year_2/Cuda%20test/examples/full_workflow_demo.cu)

# cuda_test

`cuda_test` is a C++17/CUDA library for isolated CUDA kernel development workflows.
It is designed for the common engineering loop around one kernel:

- prove correctness
- collect stable timing data
- compare launch configurations
- generate actionable diagnostics
- export results for people and tooling

The library is header-only from the consumer point of view and integrates through CMake.

## What The Library Is For

`cuda_test` is meant for developers who already write CUDA kernels and want a repeatable way to
evaluate them without building a custom harness every time.

Typical use cases:

- validate a new kernel against a CPU reference
- benchmark one kernel with warm-up and repeated measured runs
- autotune block sizes and wave multipliers
- inspect high-level bottlenecks such as transfer-heavy or block-sensitive behavior
- export results as CSV, JSON, and self-contained HTML reports
- scale from one kernel to a suite of related kernels or problem variants

## Core Capabilities

### 1. Kernel Descriptors

A `KernelDescriptor` packages everything needed to run one kernel in isolation:

- input factories
- expected output factory
- numeric tolerance
- launch callback
- default problem size

This gives the rest of the library a stable object to validate, benchmark, and tune.

### 2. Pipeline Workflow

A `Pipeline` is the main single-kernel developer workflow:

- `correctness()` checks output against the expected host-side reference
- `benchmark()` collects timing statistics over warm-up and measured runs
- `autotune()` evaluates multiple launch configurations and keeps the best one
- `diagnose()` derives simple high-level recommendations from the measured behavior

The result is a `PipelineReport`.

### 3. Suite Workflow

A `Suite` lets you batch several kernel descriptors into one run and one summary report.
This is useful when you want to compare variants:

- different problem sizes
- different coefficients or boundary conditions
- related kernels in one algorithm family

The result is a `SuiteReport`.

### 4. Reporting

The library exports results in three directions:

- `CSV` for flat tables and spreadsheets
- `JSON` for machine-readable tooling
- `HTML` for self-contained visual reports with inline CSS, inline SVG, and embedded raw JSON

## High-Level Workflow

```cpp
#include "cuda_test/cuda_test.hpp"

auto descriptor = cuda_test::describe_kernel("my_kernel")
    .problem_size(1 << 20)
    .inputs([] { return std::make_tuple(make_input_a(), make_input_b()); })
    .expected([] { return make_expected_output(); })
    .tolerance(1e-5)
    .launch([](const cuda_test::core::KernelLaunchConfig& config,
               auto& device_inputs,
               cuda_test::core::DeviceMemory<float>& output_device) {
        // launch your CUDA kernel here
    })
    .build();

auto report = cuda_test::make_pipeline(descriptor)
    .device(0)
    .correctness()
    .benchmark()
    .autotune({64, 128, 256}, {1, 2})
    .diagnose()
    .run();

report.to_html("report.html");
report.to_json("report.json");
report.to_csv("report.csv");
```

## Installation Options

The project is set up as a CMake package and also works well as a source dependency.

### Option 1. `find_package`

Use this when `cuda_test` is installed into a prefix visible to CMake.

```cmake
find_package(cuda_test 0.2.0 REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

### Option 2. `FetchContent`

This is the most practical future GitHub workflow for many consumers.

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

### Option 3. `add_subdirectory`

Use this when the library is vendored directly into your repository.

```cmake
set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/cuda_test)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

More packaging details are in [docs/integration.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/integration.md).

## Build Requirements

The repository itself expects:

- CMake 3.26+
- C++17 compiler
- CUDA toolkit for CUDA-enabled examples/tests
- Visual Studio 2022 on the current Windows setup

Host-only parts of the library still build without CUDA, but actual kernel execution paths need CUDA.

## First Demo To Run

The repository contains a full end-to-end example:

- source: [examples/full_workflow_demo.cu](/E:/source/EducationPolitech/year_2/Cuda%20test/examples/full_workflow_demo.cu)

It demonstrates:

- defining a CUDA kernel
- wrapping it in `KernelDescriptor`
- running the full `Pipeline`
- exporting HTML/JSON/CSV artifacts
- running a `Suite` over multiple descriptor variants

Generated artifacts land under:

- [reports/demo/full-workflow](/E:/source/EducationPolitech/year_2/Cuda%20test/reports/demo/full-workflow)

## Repository Guide

- [docs/getting-started.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/getting-started.md): installation and first run tutorial
- [docs/integration.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/integration.md): CMake integration patterns
- [docs/architecture/module-map.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/architecture/module-map.md): module layout
- [docs/architecture/api-boundaries.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/architecture/api-boundaries.md): subsystem boundaries
- [docs/methodology/benchmark-protocol.md](/E:/source/EducationPolitech/year_2/Cuda%20test/docs/methodology/benchmark-protocol.md): measurement conventions

## Design Intent

The project is not trying to replace Nsight Compute or Nsight Systems.
Its role is earlier and lighter-weight:

- make kernel experiments reproducible
- standardize validation and timing
- preserve results in a portable report format
- help developers make better iteration decisions before deeper profiler sessions

If the HTML report or diagnostics show a real bottleneck, the next step is usually a deeper tool,
not more guesswork.

# CUDA Test

## Authors and contributors

The main contributor recorded in the Git history of this repository is Pavel Romankov.

The repository is open to further academic and engineering contributions through issues, reviews, and pull requests.

## Introduction

`cuda_test` is a C++17/CUDA header-only library for isolated CUDA kernel validation, benchmarking, autotuning, diagnostics, and report export.

The repository contains the library source code, CMake integration, tests, benchmarks, examples, and generated demo reports. It is intended for educational and practical use when comparing CUDA kernels, checking correctness against host-side references, and exporting reproducible results.

The library packages the common engineering loop around one kernel:

- verify correctness against a host-side reference
- collect stable timing data with warm-up runs and 30 measured runs
- compare launch configurations
- generate high-level diagnostics
- export results as CSV, JSON, and self-contained HTML

## Instruction

### What the library provides

`cuda_test` is organized around seven public modules:

- `core`: CUDA runtime compatibility types, device metadata, memory helpers, launch config, and common runtime errors
- `testing`: validation helpers and the optional `cuda_test::testing` target for Google Test integration
- `benchmark`: measured-run collection and summary statistics
- `autotune`: launch candidate generation, ranking, and winner selection
- `profiling`: staged H2D / Kernel / D2H / Total timing
- `analysis`: derived metrics and bottleneck recommendations
- `reporting`: CSV / JSON / HTML export

The main public umbrella header is [`include/cuda_test/cuda_test.hpp`](include/cuda_test/cuda_test.hpp).

### Integration

`cuda_test` is packaged as an `INTERFACE` CMake target and is consumer-facing header-only.

#### `FetchContent`

This is the simplest setup once the repository is published on GitHub. Tests, benchmarks, and examples are `OFF` by default for dependency use.

```cmake
include(FetchContent)

FetchContent_Declare(
  cuda_test
  GIT_REPOSITORY https://github.com/RenjerStats/Cuda-test.git
  GIT_TAG v0.2.0
)
FetchContent_MakeAvailable(cuda_test)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

#### `add_subdirectory`

Use this when the library is vendored into another repository.

```cmake
add_subdirectory(extern/cuda_test)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

If you want the optional `cuda_test::testing` target in this mode, make `GTest::gtest` available in the parent project before `add_subdirectory(...)`.

#### `find_package`

Use this after installation into a CMake-visible prefix.

```cmake
find_package(cuda_test 0.2.0 REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

If you install the optional testing component, consumers can request it explicitly:

```cmake
find_package(cuda_test 0.2.0 REQUIRED COMPONENTS testing)
find_package(GTest REQUIRED)

target_link_libraries(my_test PRIVATE cuda_test::testing)
```

### Minimal example

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
        // Launch your CUDA kernel here.
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

### Building this repository

Requirements:

- CMake 3.26+
- a C++17 compiler
- CUDA Toolkit only when you want real CUDA execution paths

Convenience presets are already included in [`CMakePresets.json`](CMakePresets.json):

- `msvc` / `default`: host-only configuration
- `msvc-cuda` / `cuda`: CUDA-enabled configuration

Examples:

```powershell
cmake --preset default
cmake --build --preset default
```

```powershell
cmake --preset cuda
cmake --build --preset cuda
```

### Demo workflow

The repository includes an end-to-end example in [`examples/full_workflow_demo.cu`](examples/full_workflow_demo.cu).

It demonstrates:

- descriptor construction
- correctness validation
- benchmark and autotune stages
- diagnostics
- HTML / JSON / CSV export
- suite-level reporting

Generated demo artifacts live under [`reports/demo/full-workflow`](reports/demo/full-workflow).

### Notes on benchmarking

The benchmarking and autotuning flow is intentionally conservative:

- correctness is checked before performance data is accepted
- warm-up runs are executed before measured runs
- the default measured series uses 30 runs
- summary metrics include mean, median, p95, CI95, and coefficient of variation

### Repository layout

- `include/`: public headers
- `cmake/`: package config and helper modules
- `tests/`: unit, integration, and consumer smoke tests
- `examples/`: usage examples
- `benchmarks/`: benchmark runners
- `reports/demo/full-workflow/`: generated demo artifacts kept as public examples

## License

This repository is distributed under the MIT License. See [`LICENSE`](LICENSE).

The repository does not bundle third-party datasets. If external datasets are used together with this project, they remain under the original licenses specified by their respective authors and sources.

Third-party tools and libraries used during build, testing, or integration also remain under their own licenses.

## Warranty

The software is under active development and is provided on an "as is" basis. The authors provide no warranty regarding fitness for a particular purpose, correctness in every environment, or uninterrupted operation.

## References

- NVIDIA CUDA Documentation: <https://docs.nvidia.com/cuda/>
- CMake Documentation: <https://cmake.org/documentation/>
- GoogleTest Documentation: <https://google.github.io/googletest/>
- Google Benchmark repository: <https://github.com/google/benchmark>

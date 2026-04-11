# cuda_test

`cuda_test` is a C++17/CUDA header-only library for isolated CUDA kernel validation, benchmarking, autotuning, diagnostics, and report export.

The library packages the common engineering loop around one kernel:

- verify correctness against a host-side reference
- collect stable timing data with warm-up runs and 30 measured runs
- compare launch configurations
- generate high-level diagnostics
- export results as CSV, JSON, and self-contained HTML

## What The Library Provides

`cuda_test` is organized around seven public modules:

- `core`: CUDA runtime compatibility types, device metadata, memory helpers, launch config, and common runtime errors
- `testing`: validation helpers and the optional `cuda_test::testing` target for Google Test integration
- `benchmark`: measured-run collection and summary statistics
- `autotune`: launch candidate generation, ranking, and winner selection
- `profiling`: staged H2D / Kernel / D2H / Total timing
- `analysis`: derived metrics and bottleneck recommendations
- `reporting`: CSV / JSON / HTML export

The main public umbrella header is [`include/cuda_test/cuda_test.hpp`](include/cuda_test/cuda_test.hpp).

## Integration

`cuda_test` is packaged as an `INTERFACE` CMake target and is consumer-facing header-only.

### `FetchContent`

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

### `add_subdirectory`

Use this when the library is vendored into another repository.

```cmake
add_subdirectory(extern/cuda_test)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

If you want the optional `cuda_test::testing` target in this mode, make `GTest::gtest` available in the parent project before `add_subdirectory(...)`.

### `find_package`

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

## Minimal Example

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

## Building This Repository

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

## Demo Workflow

The repository includes an end-to-end example in [`examples/full_workflow_demo.cu`](examples/full_workflow_demo.cu).

It demonstrates:

- descriptor construction
- correctness validation
- benchmark and autotune stages
- diagnostics
- HTML / JSON / CSV export
- suite-level reporting

Generated demo artifacts live under [`reports/demo/full-workflow`](reports/demo/full-workflow).

## Notes On Benchmarking

The benchmarking and autotuning flow is intentionally conservative:

- correctness is checked before performance data is accepted
- warm-up runs are executed before measured runs
- the default measured series uses 30 runs
- summary metrics include mean, median, p95, CI95, and coefficient of variation

## Repository Layout

- `include/`: public headers
- `cmake/`: package config and helper modules
- `tests/`: unit, integration, and consumer smoke tests
- `examples/`: usage examples
- `benchmarks/`: benchmark runners
- `reports/demo/full-workflow/`: generated demo artifacts kept as public examples

## License

This repository is distributed under the MIT License. See [`LICENSE`](LICENSE).

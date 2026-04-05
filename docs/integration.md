# Integration

`cuda_test` is packaged as a header-only CMake project with an installable config package.

## Installed Package

```cmake
find_package(cuda_test 0.2.0 REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

The optional testing helpers live under `include/cuda_test/testing/` and depend on Google Test.
If the package was installed with the testing component enabled, consumers can request it explicitly:

```cmake
find_package(cuda_test 0.2.0 REQUIRED COMPONENTS testing)
target_link_libraries(my_test PRIVATE cuda_test::testing)
```

## FetchContent

```cmake
include(FetchContent)

set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  cuda_test
  GIT_REPOSITORY <repo_url>
  GIT_TAG v0.2.0
)
FetchContent_MakeAvailable(cuda_test)

target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

## add_subdirectory

```cmake
set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/cuda_test)
target_link_libraries(my_app PRIVATE cuda_test::cuda_test)
```

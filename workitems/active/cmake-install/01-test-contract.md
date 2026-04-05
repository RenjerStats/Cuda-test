# Test Contract

## Linkage

- Related feature packet: `workitems/active/cmake-install/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Install — headers present | `cmake --install build --prefix tmp_install` | `tmp_install/include/cuda_test/cuda_test.hpp` exists and is readable | file existence check |
| Install — config package present | same install | `tmp_install/lib/cmake/cuda_test/cuda_testConfig.cmake` exists | file existence check |
| Install — version file present | same install | `cuda_testConfigVersion.cmake` exists alongside config | file existence check |
| find_package — exact version | external CMakeLists with `find_package(cuda_test 0.2.0 EXACT)` | configure succeeds | cmake configure exit code |
| find_package — compatible version | `find_package(cuda_test 0.1.0)` | configure succeeds (0.2.0 >= 0.1.0) | cmake configure exit code |
| find_package — incompatible version | `find_package(cuda_test 999.0 REQUIRED)` | configure fails with version mismatch | cmake configure error |
| find_package — testing component | external CMakeLists with `find_package(cuda_test 0.2.0 REQUIRED COMPONENTS testing)` and test `FindGTest.cmake` shim | configure succeeds and `cuda_test::testing` is linkable | configure + build exit code |
| FetchContent — basic include | external project with FetchContent, `#include <cuda_test/cuda_test.hpp>`, prints version | configure + build succeed | build exit code |
| FetchContent — use types | external project creates `KernelLaunchConfig`, accesses fields | compile succeeds | build exit code |
| add_subdirectory — basic include | external project with `add_subdirectory(path)`, same include test | configure + build succeed | build exit code |
| Target properties | after `find_package`, inspect `cuda_test::cuda_test` | INTERFACE_INCLUDE_DIRECTORIES is set, TYPE is INTERFACE | cmake property check |
| Testing target — available with GTest | `CUDA_TEST_BUILD_TESTS=ON`, GTest found | `cuda_test::testing` target exists | cmake target check |
| Testing target — absent without GTest | `CUDA_TEST_BUILD_TESTS=OFF` | `cuda_test::testing` target not defined | cmake target absence check |
| Existing presets unbroken | `cmake --preset <existing_preset>` | configure succeeds as before | cmake configure exit code |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| No CUDA toolkit | system without CUDA, FetchContent | configure succeeds (INTERFACE library); CUDA-dependent features disabled | cmake configure succeeds with warning |
| Wrong install prefix | `find_package` without correct CMAKE_PREFIX_PATH | `find_package` fails with clear message | cmake error |

## Numerical Policy

- Not applicable — this is a build system task, no numerical computation

## Performance Scenarios

- Not applicable

## Evidence Required

- Integration test scripts:
  - `tests/integration/cmake/find_package_test/CMakeLists.txt` + `main.cpp`
  - `tests/integration/cmake/fetch_content_test/CMakeLists.txt` + `main.cpp`
  - `tests/integration/cmake/subdirectory_test/CMakeLists.txt` + `main.cpp`
  - `tests/integration/cmake/common/FindGTest.cmake` for the optional testing-component smoke test
- Manual or scripted verification:
  - `cmake --install` + `find_package` round-trip
  - FetchContent configure + build
  - add_subdirectory configure + build
- Benchmark output: none
- Report artifacts: none

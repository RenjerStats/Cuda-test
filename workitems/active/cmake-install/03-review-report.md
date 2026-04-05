# Review Report — CMake Install & FetchContent (Task 5)

## Reviewer

Claude (reviewer role)

## Review Date

2026-04-05

## Reviewed Artifacts

- `workitems/active/cmake-install/00-feature-packet.md`
- `workitems/active/cmake-install/01-test-contract.md`
- `workitems/active/cmake-install/02-implementation-notes.md`
- Diff `task/analysis-expansion..task/cmake-install` (3 commits: `spec:` → `test:` → `impl:`)

## Verdict

**Accepted** with minor observations.

No blockers. The install mechanism is correct, consumer tests are thorough, and the `cuda_test::testing` workaround is justified.

---

## 1. Alignment with Vision

| Criterion | Status | Notes |
|-----------|--------|-------|
| Solves P5 "Отсутствие механизма установки" | **Yes** | `find_package`, `FetchContent`, `add_subdirectory` all work |
| Enables `find_package(cuda_test)` | **Yes** | Config + version files installed to `lib/cmake/cuda_test/` |
| Enables `FetchContent` | **Yes** | Consumer test passes with `SOURCE_DIR` approach |
| Version check works | **Yes** | Exact, compatible, and incompatible version scenarios tested |
| No vcpkg/Conan/pkg-config (out of scope) | **Yes** | Correctly deferred |
| Parallel track — no dependencies on tasks 1-3 | **Yes** | Only touches build system, no pipeline/descriptor code |

## 2. Alignment with Пояснительная записка

- **§5** "Практичность: простая интеграция в существующий C++/CUDA проект" — directly addressed. Three standard CMake integration methods now available.
- **§10** "CMake" в техническом стеке — install mechanism is a natural extension.
- Documentation added to `docs/integration.md` with examples for all three methods.

## 3. Stage-Gate Compliance

| Gate | Present | Comment |
|------|---------|---------|
| 00-feature-packet | Yes | Clear scope, risks, acceptance criteria |
| 01-test-contract | Yes | 14 correctness scenarios + 2 negative |
| 02-implementation-notes | Yes | Deviation documented, all 7 smoke tests passed |
| 03-review-report | This document | — |
| Commits with prefixes | **Yes** | `7ff737f spec:` → `4f66567 test:` → `d4d9cc6 impl:` — correct order |

## 4. Findings

### Code / Build System

1. **minor** — `cuda_test_testing` is NOT exported via `install(TARGETS ... EXPORT ...)`. Instead, `cuda_testConfig.cmake.in` reconstructs it as an IMPORTED INTERFACE target at `find_package` time. This is documented as a deviation and has a valid root cause (FetchContent-built `gtest` cannot appear in an export set). However, it creates two independent definitions of the same target (build-tree alias vs. installed reconstruction). Any future change to `cuda_test::testing`'s link interface must be synchronized between `src/CMakeLists.txt` and `cmake/cuda_testConfig.cmake.in`.

2. **minor** — `$<BUILD_LOCAL_INTERFACE:GTest::gtest>` in `cuda_test_testing` link libraries means `add_subdirectory` consumers do NOT get `GTest::gtest` propagated through `cuda_test::testing`. The consumer must provide GTest independently. The subdirectory_test fixture does this (`find_package(GTest REQUIRED)` before `add_subdirectory`), so the test passes. This behavior is correct (avoids leaking build-tree GTest), but `docs/integration.md` could note that consumers using `add_subdirectory` + `cuda_test::testing` must have GTest in scope independently.

3. **minor** — Version bump `0.1.0 → 0.2.0` in root `CMakeLists.txt` and `cuda_test.hpp`. The parallel task/analysis-expansion branch doesn't have this bump. This will produce a merge conflict in both files when both branches merge to main. Easily resolved but worth coordinating.

4. **minor** — `install(DIRECTORY include/ ...)` copies everything under `include/`, including `.gitkeep` sentinel files. These are harmless but could be excluded with a `FILES_MATCHING PATTERN "*.hpp"` clause for a cleaner install tree.

5. **minor** — `run_consumer_smoke_test.cmake` uses `file(REMOVE_RECURSE "${FIXTURE_BINARY_DIR}")` and `file(REMOVE_RECURSE "${INSTALL_PREFIX}")` at the start of each test run. This ensures clean state but means parallel CTest execution of smoke tests could interfere if they share paths. Currently each test has a unique `${ARG_NAME}` subdirectory, so this is safe. Just noting for future awareness.

### Design

6. **observation** — The `FindGTest.cmake` shim in `tests/integration/cmake/common/` is a pragmatic solution for providing GTest to the find_package consumer test without a system-installed GTest. It creates an `UNKNOWN IMPORTED` target from the FetchContent-built artifacts. This is clever and avoids external dependencies in tests.

7. **observation** — `AnyNewerVersion` compatibility policy is appropriate for a pre-1.0 library. After 1.0, consider switching to `SameMajorVersion`.

8. **observation** — The config template correctly gates `find_dependency(CUDAToolkit)` behind `cuda_test_CUDA_ENABLED`, which is baked at install time. Since the library uses CUDA types (`dim3`) even in host-only mode via `cuda_compat.hpp`, this gating is correct — consumers only need CUDAToolkit if the installed package was CUDA-enabled.

## 5. Test Coverage Assessment

| Scenario | Status | Comment |
|----------|--------|---------|
| Install — headers present | **Passed** | Verified by `run_consumer_smoke_test.cmake` |
| Install — config + version files | **Passed** | File existence checked in script |
| find_package — exact 0.2.0 | **Passed** | `cmake_find_package_exact_test` |
| find_package — compatible 0.1.0 | **Passed** | `cmake_find_package_compatible_test` |
| find_package — incompatible 999.0 | **Passed** | `cmake_find_package_incompatible_test` (expected failure) |
| find_package — testing component | **Passed** | `cmake_find_package_testing_component_test` |
| FetchContent — basic | **Passed** | `cmake_fetch_content_test` |
| add_subdirectory — basic | **Passed** | `cmake_subdirectory_basic_test` |
| add_subdirectory — testing | **Passed** | `cmake_subdirectory_testing_target_test` |
| Target type check (INTERFACE) | **Passed** | Verified in find_package_test/CMakeLists.txt |
| Target INCLUDE_DIRECTORIES set | **Passed** | Verified in find_package_test/CMakeLists.txt |
| Version constants in code | **Passed** | Consumer `main.cpp` checks `version_minor == 2` |
| Existing presets unbroken | **Passed** | Per implementation notes, `cmake --preset msvc` works |

All 13 acceptance-relevant scenarios covered. Coverage is thorough.

## 6. Acceptance Criteria Checklist

| Criterion | Met | Evidence |
|-----------|-----|---------|
| `cmake --install` copies headers + config | Yes | Smoke test verifies file existence |
| `find_package(cuda_test)` works after install | Yes | 4 find_package test variants pass |
| `target_link_libraries` provides include path | Yes | Consumer builds with `#include <cuda_test/cuda_test.hpp>` |
| FetchContent test compiles | Yes | `cmake_fetch_content_test` |
| add_subdirectory test compiles | Yes | `cmake_subdirectory_basic_test` |
| `cuda_test::testing` available with GTest | Yes | testing component tests pass |
| `cuda_test::testing` absent without GTest | Yes | FetchContent test asserts absence |
| Version check: 0.2.0 passes, 999.0 fails | Yes | Exact + incompatible tests |
| Existing presets work | Yes | Implementation notes confirm |

## 7. Summary

| Category | Blockers | Major | Minor |
|----------|----------|-------|-------|
| Process | 0 | 0 | 0 |
| Code | 0 | 0 | 5 |
| Design | 0 | 0 | 0 |
| **Total** | **0** | **0** | **5** |

**Disposition: accepted.** Clean implementation of a standard CMake install mechanism. The `cuda_test::testing` workaround is well-justified and documented. Consumer smoke tests are comprehensive (7 CTest cases covering all three integration methods + version checks + component gating). All minor findings are non-blocking observations.

## 8. Lead Disposition

- Addressed finding 2: `docs/integration.md` now explicitly states that consumers
  using `cuda_test::testing` must have Google Test resolvable in their build, and
  that `add_subdirectory` consumers should make `GTest::gtest` available before
  linking the testing target.
- Addressed finding 4: the root install rule now uses `FILES_MATCHING PATTERN
  "*.hpp"` so the installed include tree does not pick up sentinel files or other
  non-header artifacts.
- Findings 1, 3, and 5 remain non-blocking observations:
  - the duplicated `cuda_test::testing` definition is an intentional tradeoff to
    keep installed-package consumption independent from the build-tree `gtest`
    target exported by `FetchContent`;
  - the `0.2.0` version bump merge conflict is a branch-integration concern, not
    a correctness issue within this task branch;
  - smoke-test directory cleanup is safe because each test already uses a unique
    per-test binary/install path.

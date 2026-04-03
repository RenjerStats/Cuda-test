# Feature Packet

## Task

- Name: Phase 3 - Testing / kernel-test-fixture
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/testing-fixture`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §5 (isolated unit tests), §11 (testing module), §14 (unit testing approach), §16 (integration approach), §18 (acceptance)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 2-4; stack with Google Test + CUDA Runtime API

## Objective

Реализовать минимальный testing-слой для изолированных CUDA unit-тестов. Результат фазы должен дать базовый `KernelTestFixture`, набор reusable validation helpers и пример полного теста с тривиальным `vector add` kernel, чтобы последующие фазы могли опираться на единый шаблон проверки корректности.

## Scope

- In scope:
  - `KernelTestFixture` как базовый GTest fixture для CUDA-тестов
  - `expect_array_near` и `expect_array_eq` с диагностикой индекса
  - reusable vector-add fixture builder under `tests/fixtures/`
  - пример CUDA unit-теста с host reference (`vector add`)
  - CMake registration for testing module tests
  - warning-policy adjustment if needed for `.cu` Google Test targets

- Out of scope:
  - DSL macros like `CUDA_EXPECT_ARRAY_NEAR`
  - generic kernel launcher abstraction
  - benchmark/autotune integration
  - complex multi-kernel fixtures from the course project
  - inclusion of GTest-dependent headers into the general-purpose `cuda_test.hpp`

## Affected Areas

- Modules: `testing`
- Public headers:
  - `include/cuda_test/testing/kernel_test_fixture.hpp`
  - `include/cuda_test/testing/validation.hpp`
- Internal components:
  - `tests/fixtures/vector_add_fixture.hpp`
  - `tests/unit/testing/`
  - `tests/CMakeLists.txt`
  - `cmake/Warnings.cmake` if `.cu` compile flags need narrowing for nvcc
- Reports or methodology: none

## Interface Notes

- New or changed types:
  - `class KernelTestFixture : public ::testing::Test`
  - optional helper struct for vector-add fixture data under `tests/fixtures/`
- New or changed functions:
  - `expect_array_eq(const T* actual, const T* expected, std::size_t n)`
  - `expect_array_near(const T* actual, const T* expected, std::size_t n, T eps)`
  - fixture helpers for device setup and 1D launch config
- Input or output assumptions:
  - testing headers are public, but they intentionally depend on GTest and are not re-exported from `cuda_test.hpp`
  - validation helpers must produce index-aware failure messages
  - kernel correctness tests must use deterministic host-side references

## Risks

- Technical risks:
  - leaking GTest dependency into non-test consumers through umbrella includes; mitigation: keep testing headers separate from `cuda_test.hpp`
  - CUDA `.cu` test targets under MSVC may fail if warning flags are passed through nvcc; mitigation: adjust warning policy when needed
- Measurement risks:
  - not applicable; this phase validates correctness, not performance
- Integration risks:
  - fixture API may be too narrow for later real kernels; mitigation: keep Phase 3 focused on reusable primitives (device selection, launch config, validation)

## Acceptance Criteria

- [ ] `include/cuda_test/testing/` contains public fixture and validation headers
- [ ] `expect_array_eq` and `expect_array_near` produce index-aware GTest failures
- [ ] `KernelTestFixture` supports selecting a CUDA device and computing a 1D launch config
- [ ] vector-add CUDA unit test passes against a deterministic host reference
- [ ] host-only configure remains valid and CUDA test build succeeds in `msvc-cuda`

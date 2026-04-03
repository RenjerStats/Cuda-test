# Test Contract

## Linkage

- Related feature packet: `workitems/active/testing-fixture/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| `expect_array_eq` pass | identical integer arrays, `n=8` | no failure | direct GTest execution |
| `expect_array_eq` fail | one differing element | non-fatal failure includes failing index | `EXPECT_NONFATAL_FAILURE` |
| `expect_array_near` pass | float arrays within `eps=1e-5` | no failure | direct GTest execution |
| `expect_array_near` fail | float arrays differing by more than `eps` | non-fatal failure includes failing index and delta context | `EXPECT_NONFATAL_FAILURE` |
| `KernelTestFixture` launch config | `N=1024`, block size `128` | grid.x=`8`, block.x=`128` | exact field equality |
| `KernelTestFixture` vector-add kernel | `N=1024` | device output equals host reference | `expect_array_near`, `eps=1e-5` |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Invalid device selection | request non-existent device id | throws `std::out_of_range` |
| Zero block size | ask fixture for 1D launch config with `block_size=0` | throws `std::invalid_argument` |
| Validation null mismatch | non-zero length with null pointer input | throws `std::invalid_argument` |

## Numerical Policy

- Tolerance strategy: `expect_array_eq` uses exact equality; `expect_array_near` uses explicit `eps=1e-5f` in the vector-add test
- Deterministic seed strategy: vector-add inputs are monotonic deterministic vectors, no RNG
- Host reference implementation: vector-add reference is computed on CPU in reusable fixture helper

## Performance Scenarios

- Warm-up runs: not applicable
- Measured runs: not applicable
- Input sizes: `8`, `1024`
- Metrics to capture: none
- Baseline comparison: not applicable

## Evidence Required

- Unit or integration tests: `tests/unit/testing/` covers validation helpers, launch config helper, and vector-add kernel correctness
- Benchmark output: none for this phase
- Report artifacts: none for this phase

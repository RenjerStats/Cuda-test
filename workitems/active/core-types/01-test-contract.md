# Test Contract

## Linkage

- Related feature packet: `workitems/active/core-types/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| `KernelLaunchConfig` defaults | default-constructed object | `grid=(1,1,1)`, `block=(1,1,1)`, `shared_mem=0`, `device_id=0` | exact field equality |
| `ProfilingBreakdown` defaults | default-constructed object | all stage timings are `0.0` | exact field equality |
| `RunStats` defaults | default-constructed object | all metrics are `0.0` | exact field equality |
| `DeviceMemory<float>` alloc/free | `N=1024` | allocation succeeds, `size()==1024`, pointer non-null | assertions + no throw |
| `DeviceMemory<float>` round-trip | host vector `1..1024` | values after H2D + D2H match original | element-wise exact equality |
| `DeviceMemory<float>` zero-size | `N=0` | object is valid, `size()==0`, pointer null, no crash | assertions + no throw |
| `DeviceMemory<float>` move semantics | move-construct and move-assign populated buffer | ownership transferred, source reset to empty | field/state assertions |
| `device_exists()` happy path | current device id or device 0 | returns `true` when at least one device exists | runtime check |
| `get_device_info()` | valid device id | returns non-empty name and positive device limits | assertions on fields |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Invalid device id | query `device_exists(-1)` or `get_device_info(large_id)` | `device_exists()` returns `false`; `get_device_info()` throws |
| CUDA error propagation | call `check_cuda(cudaErrorInvalidValue, ...)` | throws exception with expression/file/line context |
| Copy size mismatch | copy host buffer with smaller size than device allocation | throws `std::invalid_argument` |

## Numerical Policy

- Tolerance strategy: exact equality for phase 1; floating-point tolerance is not needed because no arithmetic kernels are executed
- Deterministic seed strategy: fixed monotonic host fixtures (`1..N`) and no random generation
- Host reference implementation: host vector round-trip acts as the reference for memory copy tests

## Performance Scenarios

- Warm-up runs: not applicable in this phase
- Measured runs: not applicable in this phase
- Input sizes: `0`, `1`, `1024`
- Metrics to capture: functional state only; no timing evidence
- Baseline comparison: not applicable

## Evidence Required

- Unit or integration tests: `tests/unit/core/` covering defaults, errors, device helpers, and `DeviceMemory`
- Benchmark output: none for this phase
- Report artifacts: none for this phase

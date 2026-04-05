# Test Contract

## Linkage

- Related feature packet: `workitems/active/kernel-descriptor/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Builder fluent API — all fields set | name="test", problem_size=1024, inputs/expected/launch provided | `KernelDescriptor` constructs without error; `name()=="test"`, `problem_size()==1024` | field assertions |
| Builder — missing field | omit `launch` or `inputs` | compilation error or `build()` throws | static_assert or exception test |
| `baseline_config()` default | problem_size=1024, device_id=0 | `block.x==128`, `grid.x==8` (1024/128), `shared_mem==0` | exact field equality |
| `baseline_config()` non-aligned | problem_size=1000, device_id=0 | `block.x==128`, `grid.x==8` (ceil(1000/128)), `grid.x * block.x >= 1000` | grid covers problem |
| `validate()` — correct kernel (CUDA) | vector_add: a=[1..N], b=[2..N+1], expected=[3..2N+1], tol=1e-5 | returns `true` | boolean assertion |
| `validate()` — wrong expected (CUDA) | vector_add with deliberately wrong expected | returns `false` | boolean assertion |
| `validate()` — exact match int (CUDA) | identity kernel copying int input to output | returns `true` when expected matches, `false` otherwise | boolean assertion |
| `measure()` — non-trivial (CUDA) | vector_add with N=65536 | returns `ProfilingBreakdown` with all 4 stages > 0 | all fields positive |
| `measure()` — stages ordered | vector_add with N=65536 | `total_ms >= kernel_ms` and `total_ms >= h2d_ms` and `total_ms >= d2h_ms` | numerical inequality |
| `measure()` — repeated calls | call measure() twice on same descriptor/config | both return valid non-zero breakdowns (descriptor reusable) | assertions on both results |
| Descriptor move semantics | move-construct a KernelDescriptor | moved-to object retains name, problem_size, validate/measure work | field + functional assertions |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Zero problem size | `problem_size(0)` | `baseline_config()` returns grid.x=0; `validate()` returns `true` (trivial); `measure()` returns breakdown with zero or near-zero times |
| Kernel launch error (CUDA) | launch callable sets grid=0 or passes invalid config | `validate()` throws or returns false; `measure()` throws (CUDA error propagated via CUDA_CHECK) |
| Device unavailable | `baseline_config(device_id=999)` on machine with 1 GPU | throws exception from device selection |

## Numerical Policy

- Tolerance strategy: float scenarios use `tolerance(1e-5f)`; int scenarios use exact match (no `.tolerance()` call)
- Deterministic seed strategy: fixed monotonic host data (e.g., `a[i]=float(i)`, `b[i]=float(i+1)`)
- Host reference implementation: expected computed on host before descriptor construction

## Performance Scenarios

- Warm-up runs: not applicable — descriptor itself is not benchmarked
- Measured runs: not applicable
- Input sizes for CUDA tests: `1024`, `65536`
- Metrics to capture: functional correctness only; `measure()` returns timing but we only verify positivity
- Baseline comparison: not applicable

## Evidence Required

- Host-only unit tests: `tests/unit/pipeline/kernel_descriptor_test.cpp` — builder API, name, problem_size, baseline_config, move semantics
- CUDA integration tests: `tests/integration/pipeline/kernel_descriptor_cuda_test.cu` — validate(), measure() with real vector_add kernel
- Benchmark output: none for this task
- Report artifacts: none for this task

# Test Contract

## Linkage

- Related feature packet: `workitems/active/analysis-expansion/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| `estimate_occupancy` — valid kernel (CUDA) | real vector_add kernel, block_size=128 | `active_blocks_per_sm > 0`, `active_warps_per_sm > 0`, `occupancy_ratio` in (0, 1] | range assertions |
| `estimate_occupancy` — block_size=1 (CUDA) | block_size=1 | valid result with positive warp/block counts | positivity assertion |
| `estimate_occupancy` — max block (CUDA) | block_size=1024 | valid result, `occupancy_ratio` in (0, 1] | range assertion |
| `suggest_block_size` (CUDA) | real vector_add kernel symbol | returns value > 0, multiple of warp_size (32) | divisibility + positivity |
| `get_kernel_attributes` (CUDA) | real vector_add kernel | `num_regs > 0`, `max_threads_per_block > 0`, `max_dynamic_shared_size_bytes >= 0` | positivity assertions |
| `estimate_bandwidth` — known values | bytes=1GB (1e9), time=10ms, device=0 | `achieved_gbps = 100.0`, `theoretical_gbps > 0`, `utilization_ratio > 0` | exact achieved + positivity |
| `estimate_bandwidth` — small transfer | bytes=1024, time=0.001ms | `achieved_gbps` reasonable, `utilization_ratio` small | positivity + range |
| `query_memory_pressure` (CUDA) | device_id=0 | `total_bytes > 0`, `free_bytes <= total_bytes`, `usage_ratio` in [0, 1] | range assertions |
| `block_sensitivity` — identical candidates | 3 candidates all with median=5.0 | returns `1.0` | exact equality |
| `block_sensitivity` — diverse candidates | candidates with medians [2.0, 4.0, 6.0] | returns `3.0` (6.0/2.0) | exact equality |
| `block_sensitivity` — single candidate | 1 candidate with median=3.0 | returns `1.0` | exact equality |
| `scaling_exponent` — linear data | pairs: (100,1), (1000,10), (10000,100) | returns `~1.0` (±0.05) | near equality |
| `scaling_exponent` — quadratic data | pairs: (10,100), (100,10000), (1000,1000000) | returns `~2.0` (±0.05) | near equality |
| `scaling_exponent` — constant data | pairs: (100,5), (1000,5), (10000,5) | returns `~0.0` (±0.05) | near equality |
| `transfer_compute_ratio` — unchanged | breakdown: h2d=2, kernel=4, d2h=1 | returns `0.75` | exact equality |
| `KernelFingerprint` — construction | all fields set manually, including local/shared memory fields | all fields accessible with set values | field assertions |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| `estimate_occupancy` — null kernel | `kernel_func=nullptr` | throws `std::invalid_argument` |
| `estimate_occupancy` — invalid block size | `block_size <= 0` | throws `std::invalid_argument` |
| `estimate_bandwidth` — zero time | `kernel_ms=0.0` | throws `std::invalid_argument` (division by zero) |
| `estimate_bandwidth` — zero bytes | `bytes=0, time=1.0` | returns `achieved_gbps=0.0`, `utilization_ratio=0.0` |
| `block_sensitivity` — empty candidates | empty vector | throws `std::invalid_argument` |
| `scaling_exponent` — less than 2 points | 1 pair or empty | throws `std::invalid_argument` |
| `scaling_exponent` — zero N | pair with N=0 | throws (log(0) undefined) |
| `get_kernel_attributes` — null kernel | `kernel_func=nullptr` | throws `std::invalid_argument` |
| `query_memory_pressure` — invalid device | device_id=999 | throws exception |

## Numerical Policy

- Tolerance strategy: host-only metric computations use exact equality where possible (bandwidth with known inputs). Scaling exponent and occupancy use tolerance ±0.05 due to floating-point regression and device-dependent behavior.
- Deterministic seed strategy: synthetic pairs for scaling_exponent; fixed known values for bandwidth
- Host reference implementation: manually computed expected values for bandwidth and sensitivity

## Performance Scenarios

- Not applicable for this phase. Metrics themselves are queries, not benchmarks.
- CUDA tests should be fast: occupancy/attributes are single API calls, memory_info is a query.

## Evidence Required

- Host-only unit tests: `tests/unit/analysis/bandwidth_test.cpp` — estimate_bandwidth with known values
- Host-only unit tests: `tests/unit/analysis/metrics_extended_test.cpp` — block_sensitivity, scaling_exponent, fingerprint construction
- CUDA integration tests: `tests/integration/analysis/analysis_cuda_runtime_test.cu` — estimate_occupancy, suggest_block_size, get_kernel_attributes, query_memory_pressure with real kernel
- Benchmark output: none
- Report artifacts: none

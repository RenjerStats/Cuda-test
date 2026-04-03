# Test Contract

## Linkage

- Related feature packet: `workitems/archive/mvp/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

Каждая фаза имеет свой набор сценариев. Ниже — сводный контракт уровня MVP.

### Phase 1 — core-types

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| KernelLaunchConfig default | default-constructed | grid=(1,1,1), block=(1,1,1), shared_mem=0, device_id=0 | exact equality |
| RunStats from known data | {1.0, 2.0, 3.0, ..., 30.0} | mean=15.5, median=15.5, known p95/ci95 | tolerance ±0.01 |
| DeviceMemory alloc/free | N=1024, float | no leak, size()==1024 | CUDA_CHECK + size check |
| DeviceMemory H2D/D2H roundtrip | vector<float> {1,2,3,...,1024} | identical after roundtrip | exact element-wise |
| DeviceMemory zero-size | N=0 | valid empty object, no crash | no-throw |

### Phase 2 — profiling-timer

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| Timer smoke | trivial kernel, N=256 | all stages ≥ 0.0 ms, total ≥ kernel | inequality check |
| Timer consistency | same kernel ×10 | cv < 0.5 across runs | stats check |

### Phase 3 — testing-fixture

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| expect_array_near pass | identical arrays, eps=1e-5 | pass | GTest EXPECT |
| expect_array_near fail | diff > eps | failure message with index | GTest EXPECT_NONFATAL_FAILURE |
| KernelTestFixture vectorAdd | N=1024, a[i]+b[i] | matches host reference | expect_array_near, eps=1e-5 |

### Phase 4 — benchmark-runner

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| Stats from constant data | 30× same value | mean==median==value, cv≈0, ci95 tight | tolerance ±0.001 |
| Stats from known distribution | 30 linearly spaced | match precomputed mean/median/p95 | tolerance ±0.01 |
| Runner warm-up excluded | warmup=5, measure=30 | exactly 30 samples in result | count check |

### Phase 5 — autotune-search

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| Single candidate | 1 block_size | that config is winner | exact equality |
| Ranking correctness | 3 mock candidates with known median | winner = lowest median | exact equality |
| Invalid candidate filtered | block_size > device max threads | excluded from results | not in all_candidates |

### Phase 6 — analysis + reporting

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| transfer_compute_ratio | h2d=1, kernel=2, d2h=1 | ratio = 1.0 | exact equality |
| CSV round-trip | AutoTuneResult with 3 candidates | parse back, values match | field-by-field tolerance ±0.001 |
| JSON round-trip | same | parse back, values match | field-by-field tolerance ±0.001 |
| CSV empty result | 0 candidates | valid file, header only | file exists + header check |

### Phase 7 — kernel-integration

| Scenario | Input shape | Expected result | Validation method |
|---|---|---|---|
| Each of 6 kernels — correctness | representative N | matches host reference | expect_array_near per kernel |
| Each of 6 kernels — small input | minimal meaningful N | same as above | expect_array_near |
| Each of 6 kernels — autotune | representative N, all block candidates | best config selected, reason filled | RunStats + reason non-empty |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
|---|---|---|
| Invalid device_id | device_id = 99 | check_cuda throws / returns error |
| cudaMalloc failure | absurdly large N | check_cuda throws |
| Kernel launch failure | block > maxThreadsPerBlock | CUDA error detected after launch |
| Autotune with no valid candidates | all block_sizes invalid for device | AutoTuneResult with empty candidates, error/warning |

## Numerical Policy

- Tolerance strategy: `eps = 1e-5` для float, `eps = 1e-10` для double, если feature packet конкретного ядра не переопределяет.
- Deterministic seed strategy: фиксированный seed для генерации входных данных; один seed на тест-кейс; значение seed фиксируется в fixture.
- Host reference implementation: для каждого ядра должна быть наивная CPU-реализация в `tests/fixtures/` или inline в тесте.

## Performance Scenarios

- Warm-up runs: 5
- Measured runs: 30
- Input sizes: minimal (256), representative (65536), large (1M) — конкретные значения зависят от ядра
- Metrics to capture: `mean_ms`, `median_ms`, `p95_ms`, `ci95_low`, `ci95_high`, `cv`, полный `ProfilingBreakdown`
- Baseline comparison: default launch config (block=256, grid=ceil(N/256)) vs autotune winner

## Evidence Required

- Unit or integration tests: все сценарии выше проходят в `ctest`
- Benchmark output: CSV/JSON файлы в `reports/` для каждого из 6 ядер
- Report artifacts: сводная таблица baseline vs autotune на 2 GPU


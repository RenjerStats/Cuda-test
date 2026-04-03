# Test Contract

## Linkage

- Related feature packet: `workitems/archive/kernel-integration/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Density update kernel | representative `N`, deterministic `density + delta` inputs | device output matches host reference | element-wise `EXPECT_NEAR`, `eps=1e-5f` |
| Physics integration kernel | representative `N`, `position + velocity * dt` | device output matches host reference | element-wise `EXPECT_NEAR`, `eps=1e-5f` |
| Contact flag kernel | representative `N`, paired scalar arrays + threshold | device flags match host reference | exact equality |
| Active compaction kernel | representative `N`, deterministic active mask | compacted indices and active count match host reference | exact equality |
| Buffer generation kernel | representative `N`, deterministic source values | generated buffer matches host reference | element-wise `EXPECT_EQ` |
| Intersection flag kernel | representative `N`, interval arrays + query point | output flags match host reference | exact equality |

## Boundary And Launch Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Smallest meaningful inputs | minimal non-empty fixture per kernel | output still matches host reference | same comparison policy as the representative scenario |
| Launch handling | representative `N`, baseline block size `128` | computed `grid.x` covers `N` and kernel executes successfully | launch config equality + CUDA status |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| No CUDA device | integration runner starts without visible CUDA GPU | test or executable exits early with a clear skip/error message |
| Invalid autotune candidates | candidate list contains oversized block sizes | oversized candidates are excluded before measurement |
| Missing second GPU | environment exposes only one NVIDIA GPU | phase records the second-GPU run as deferred, not completed |

## Numerical Policy

- Tolerance strategy: `eps=1e-5f` for floating-point kernels; exact equality for integer / flag outputs
- Deterministic seed strategy: no RNG; all kernel fixtures are literal or arithmetic sequences
- Host reference implementation: every kernel case has an explicit CPU reference implementation in `tests/fixtures/`

## Performance Scenarios

- Warm-up runs: 5
- Measured runs: 30
- Input sizes: one small fixture and one representative fixture per kernel; autotune/evidence generation uses the representative fixture
- Metrics to capture: stage `RunStats` for `h2d`, `kernel`, `d2h`, `total`, plus winner reason and `transfer_compute_ratio`
- Baseline comparison: fixed baseline `block=128`, `grid_wave_multiplier=1` vs autotune winner per kernel

## Evidence Required

- Unit or integration tests: `tests/integration/` correctness tests for the six kernels
- Benchmark output: one runner in `benchmarks/kernels/` generates per-kernel CSV/JSON outputs
- Report artifacts: generated files under `reports/tables/` for the single available GPU plus a summary file that lists the six kernels and their winning configurations


# Feature Packet

## Task

- Name: Post-MVP Phase 6 - Diagnostics Advisor
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/diagnostics-advisor`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` - "Recommendation system"

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`:
  - Section 7 - early bottleneck detection
  - Section 15 - bottleneck metrics and interpretation
  - Section 20 - extension toward actionable recommendations
- Related section(s) in vision:
  - "Recommendation system"
  - P4 diagnostics
- External references used to tighten the rule set:
  - NVIDIA CUDA C++ Best Practices Guide 12.6.1:
    - 8.2 Bandwidth
    - 9.1 Data Transfer Between Host and Device
    - 9.2 Device Memory Spaces
    - 10.1 Occupancy
    - 10.3 Thread and Block Heuristics
    - 10.6 Multiple contexts
  - NVIDIA CUDA Runtime API:
    - Occupancy API
    - `cudaFuncAttributes`

## Objective

Implement a rule-based diagnostics layer that converts `analysis::KernelFingerprint`
into concrete optimization advice. Integrate that diagnostics stage into the
pipeline through `.diagnose()`, so the library explains likely bottlenecks instead
of only reporting timings.

The key product goal is not to replace Nsight Compute. It is to turn the runtime
metrics already available in this project into high-signal first-pass guidance for
developers who are not CUDA performance specialists.

## Scope

- In scope:
  - New public header `include/cuda_test/analysis/advisor.hpp`
  - `analysis::Recommendation`:
    - `tag`
    - `severity`
    - `summary`
    - `suggestion`
  - `analysis::diagnose(const KernelFingerprint&) -> std::vector<Recommendation>`
  - Rule set grounded in the available metrics:
    1. `transfer_dominated`
    2. `low_occupancy`
    3. `local_memory_pressure`
    4. `bandwidth_bound`
    5. `unstable_timing`
    6. `block_sensitive`
    7. `superlinear_scaling`
    8. `well_utilized`
  - Pipeline integration:
    - `Pipeline::diagnose()`
    - `PipelineReport` gains `diagnose_enabled`
    - `PipelineReport` gains `std::optional<analysis::KernelFingerprint> fingerprint`
    - `PipelineReport` gains `std::vector<analysis::Recommendation> recommendations`
  - Reporting integration:
    - JSON export includes `fingerprint` and `recommendations`
    - CSV export includes summary columns for fingerprint fields and recommendation tags/count
  - Tests:
    - host-only rule tests for `diagnose(fp)`
    - CUDA pipeline integration test for `.diagnose()`
    - reporting tests for pipeline JSON/CSV with diagnostics fields

- Out of scope:
  - Nsight-only metrics such as cache hit rates, warp execution efficiency, or instruction mix
  - ML-based or profile-history-based recommendations
  - Automatic source rewrites
  - User-defined custom rules
  - Full occupancy / register / local-memory capture inside `Pipeline` until the
    descriptor layer carries a raw kernel symbol
  - `bandwidth_utilization` inside `Pipeline` until the pipeline can supply bytes transferred
  - `scaling_exponent` inside `Pipeline` until the pipeline supports multi-size experiments

## Affected Areas

- Modules:
  - `analysis`
  - `pipeline`
  - `reporting`
- Public headers:
  - `include/cuda_test/analysis/advisor.hpp` (new)
  - `include/cuda_test/analysis/fingerprint.hpp` (existing dependency)
  - `include/cuda_test/pipeline/pipeline.hpp`
  - `include/cuda_test/pipeline/pipeline_report.hpp`
  - `include/cuda_test/reporting/export.hpp`
  - `include/cuda_test/cuda_test.hpp`
- Tests:
  - `tests/unit/analysis/`
  - `tests/unit/reporting/`
  - `tests/integration/pipeline/`

## Interface Notes

### Recommendation type

```cpp
namespace cuda_test::analysis {

struct Recommendation {
    std::string tag;
    std::string severity;   // "info", "warning", "critical"
    std::string summary;
    std::string suggestion;
};

std::vector<Recommendation> diagnose(const KernelFingerprint& fingerprint);

} // namespace cuda_test::analysis
```

### Rule set

The rules are intentionally conservative. The advice should be defensible from the
metrics we actually have, not from metrics that would require Nsight Compute.

| Tag | Condition | Severity | Intent |
| --- | --- | --- | --- |
| `transfer_dominated` | `transfer_compute_ratio > 3.0` | `critical` | Host/device traffic dominates useful kernel work |
| `low_occupancy` | `occupancy < 0.5 && num_regs > 32` | `warning` | Latency hiding is likely constrained by register usage |
| `local_memory_pressure` | `local_size_bytes > 0` | `warning` | Per-thread local memory is off-chip and often indicates spills or oversized locals |
| `bandwidth_bound` | `bandwidth_utilization > 0.8` | `info` | Kernel is near the available device-memory bandwidth |
| `unstable_timing` | `cv > 0.15` | `warning` | Measurements are noisy enough that conclusions are less trustworthy |
| `block_sensitive` | `block_sensitivity > 1.3` | `info` | Launch shape materially affects performance |
| `superlinear_scaling` | `scaling_exponent > 1.2` | `warning` | Runtime grows faster than near-linear expectations |
| `well_utilized` | no other rule fired, `transfer_compute_ratio < 0.3`, `occupancy >= 0.6`, `cv < 0.1`, `block_sensitivity <= 1.2` | `info` | No obvious high-level bottleneck from the available metrics |

### Advice policy behind each rule

- `transfer_dominated`:
  - Official NVIDIA basis:
    - minimize host/device transfers
    - batch transfers
    - use pinned memory for higher bandwidth
    - overlap copies and compute with asynchronous operations when appropriate
  - Advice should prioritize:
    - keeping intermediates on the device
    - batching small transfers
    - pinned memory
    - copy/compute overlap in non-default streams

- `low_occupancy`:
  - Official NVIDIA basis:
    - occupancy is the ratio of active warps to maximum warps
    - low occupancy hurts latency hiding
    - higher occupancy does not always improve performance
    - register usage can reduce occupancy
  - Advice should mention:
    - `--ptxas-options=-v`
    - `__launch_bounds__`
    - `-maxrregcount`
    - re-check block sizes in the 128-256 range and multiples of 32

- `local_memory_pressure`:
  - Official NVIDIA basis:
    - local memory is off-chip
    - local memory often appears when register space is insufficient or when large / dynamically indexed locals are used
  - Advice should mention:
    - reducing large per-thread arrays / structs
    - reducing spills / live ranges
    - inspecting PTX or ptxas `lmem` output

- `bandwidth_bound`:
  - Official NVIDIA basis:
    - compare effective bandwidth to theoretical bandwidth
    - coalescing is high priority
    - shared memory is useful to eliminate redundant reads and to transform strided accesses
    - shared-memory bank conflicts are expensive
  - Advice should mention:
    - coalesced global memory access
    - removing redundant global traffic
    - shared memory as a user-managed cache
    - padding tiled shared-memory layouts when bank conflicts are likely

- `unstable_timing`:
  - Official NVIDIA basis:
    - multiple CUDA contexts on one GPU are time-sliced
    - context switching reduces utilization
    - exclusive-process mode can restrict a GPU to one context
  - Advice should mention:
    - reducing background GPU activity
    - avoiding competing contexts/processes
    - exclusive-process mode for controlled benchmarking
    - increasing sample count only after reducing external noise

- `block_sensitive`:
  - Official NVIDIA basis:
    - exact block-size choice requires experimentation
    - multiples of 32 are preferred
    - 128-256 threads per block are a good initial range
  - Advice should mention:
    - autotuning after kernel changes
    - keeping launch shape configurable
    - starting exploration from 128-256 threads and multiples of 32

- `superlinear_scaling`:
  - Direct NVIDIA basis:
    - scaling analysis should distinguish weak/strong scaling and serial fractions
  - Inference used in this task:
    - if measured time grows materially faster than linear, inspect algorithmic complexity,
      serial work, contention, and traffic growth

- `well_utilized`:
  - This is intentionally weak and only fires when the other rules stay silent.
  - It is not a claim of optimality, only a claim that no obvious high-level bottleneck
    was detected from the metrics available in this library.

### Pipeline integration

```cpp
auto report = cuda_test::make_pipeline(descriptor)
    .benchmark()
    .autotune({64, 128, 256})
    .diagnose()
    .run();
```

`PipelineReport` additions:

```cpp
class PipelineReport {
public:
    bool diagnose_enabled() const noexcept;
    const std::optional<analysis::KernelFingerprint>& fingerprint() const noexcept;
    const std::vector<analysis::Recommendation>& recommendations() const noexcept;
};
```

### Pipeline v1 limitations

- When `.diagnose()` is used inside `Pipeline`, the timing source is:
  - the explicit benchmark stage if present
  - otherwise the winning autotune candidate if present
- `Pipeline` v1 can populate:
  - `transfer_compute_ratio`
  - `cv`
  - `block_sensitivity`
- `Pipeline` v1 cannot populate automatically:
  - `occupancy`
  - `bandwidth_utilization`
  - `num_regs`
  - `local_size_bytes`
  - `shared_size_bytes`
  - `scaling_exponent`
- Therefore, the only rules guaranteed to be meaningful inside `Pipeline` v1 are:
  - `transfer_dominated`
  - `unstable_timing`
  - `block_sensitive`
- The remaining rules still work for standalone `diagnose(fp)` when the caller
  provides a fully populated `KernelFingerprint`.

## Risks

1. Thresholds such as `3.0`, `0.15`, and `1.3` are heuristics, not laws of physics.
   Mitigation: document them as first-pass rules, keep them centralized, and test exact boundaries.
2. Occupancy is easy to over-interpret.
   Mitigation: do not equate high occupancy with peak performance; only use it as a latency-hiding signal.
3. Pipeline diagnostics can only use the subset of metrics that the current
   descriptor/pipeline layer can actually provide.
   Mitigation: document the limitation explicitly and avoid pretending that missing metrics are known.
4. Advice can become contradictory if positive and negative rules fire together.
   Mitigation: make `well_utilized` fire only when no other rule fires.

## Acceptance Criteria

- [ ] `diagnose(fp)` returns an empty vector for a healthy fingerprint
- [ ] `diagnose(fp)` returns `transfer_dominated` for `transfer_compute_ratio = 5.0`
- [ ] `diagnose(fp)` returns `low_occupancy` for `occupancy = 0.3` and `num_regs = 48`
- [ ] `diagnose(fp)` returns `local_memory_pressure` for `local_size_bytes > 0`
- [ ] `diagnose(fp)` returns `bandwidth_bound` for `bandwidth_utilization = 0.9`
- [ ] `diagnose(fp)` returns `unstable_timing` for `cv = 0.25`
- [ ] `diagnose(fp)` returns `block_sensitive` for `block_sensitivity = 2.0`
- [ ] `diagnose(fp)` returns `superlinear_scaling` for `scaling_exponent = 1.5`
- [ ] `diagnose(fp)` returns `well_utilized` only when no other rule fires
- [ ] Multiple simultaneous findings produce multiple recommendations
- [ ] Every recommendation has non-empty `tag`, `severity`, `summary`, and `suggestion`
- [ ] `Pipeline::diagnose()` preserves graceful behavior when no timing data is available
- [ ] `Pipeline::diagnose()` populates `report.fingerprint()` when benchmark or autotune data is available
- [ ] `Pipeline::diagnose()` populates `report.recommendations()`
- [ ] Pipeline JSON export includes `fingerprint` and `recommendations`
- [ ] Pipeline CSV export includes diagnostics summary columns
- [ ] Host-only and CUDA test presets continue to build the affected targets

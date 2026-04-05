# Review Report — Analysis Expansion (Task 4)

## Reviewer

Claude (reviewer role)

## Review Date

2026-04-05

## Reviewed Artifacts

- `workitems/active/analysis-expansion/00-feature-packet.md`
- `workitems/active/analysis-expansion/01-test-contract.md`
- `workitems/active/analysis-expansion/02-implementation-notes.md`
- Working-tree diff (uncommitted): new headers, extended `metrics.hpp`, tests, CMake/umbrella updates

## Verdict

**Accepted** with minor observations.

No blockers. Implementation matches packet scope, aligns with vision, and code quality is clean.

---

## 1. Alignment with Vision

| Criterion | Status | Notes |
|-----------|--------|-------|
| Solves P4 "Скудная аналитика" | **Yes** | All Runtime API metrics from the vision table are implemented |
| Stays within Layer 2 | **Yes** | No Layer 3 (Pipeline) coupling; `build_fingerprint` takes primitives, not orchestration types |
| Prepares KernelFingerprint for diagnostics-advisor | **Yes** | Struct matches vision's design; all fields present |
| Defers interpretation to task 6 | **Yes** | No rule-based logic, no recommendations — pure metric collection |
| Module boundary respected | **Yes** | Only `analysis` module touched; no new top-level modules |

## 2. Alignment with Пояснительная записка

- **§15** lists three derived metrics: `transfer_compute_ratio`, `coalescing_sensitivity`, `scaling_exponent`. Implementation covers TCR and scaling exponent. `coalescing_sensitivity` absent — see minor #3 below.
- **§7** "автоматическое снятие метрик" — directly enabled by this expansion.
- **§3 задача 5** "Реализовать метрики оценки узких мест" — directly addressed.
- **§4** boundaries respected: no Nsight Compute / CUPTI usage.

## 3. Stage-Gate Compliance

| Gate | Present | Comment |
|------|---------|---------|
| 00-feature-packet | Yes | Clear scope, risks, acceptance criteria |
| 01-test-contract | Yes | Correctness + negative + numerical policy |
| 02-implementation-notes | Yes | Deviations documented, validation commands listed, all tests pass |
| 03-review-report | This document | — |
| Commits with prefixes | **No** | See process finding below |

## 4. Findings

### Process

**major** — Changes are uncommitted. The branch `task/analysis-expansion` points to the same commit as `task/export-refactor`. All new files (7 headers, 3 test files) and modifications (`metrics.hpp`, `cuda_test.hpp`, `tests/CMakeLists.txt`) are untracked or unstaged. The stage-gate requires `spec:` → `test:` → `impl:` commits before review acceptance. Recommendation: commit in the proper sequence before merging.

### Code

1. **minor** — `max_median` in `block_sensitivity()` ([metrics.hpp:29](include/cuda_test/analysis/metrics.hpp#L29)) initialized to `0.0`. Works correctly because the `median_ms <= 0.0` guard above guarantees all values are positive. However, initializing to `-std::numeric_limits<double>::infinity()` would be more idiomatic for a max-finding loop and would not rely on the order of validation vs. accumulation.

2. **minor** — Cross-module dependency: `metrics.hpp` now includes `autotune/search.hpp`, creating `analysis → autotune` coupling. Both are Layer 2, so this is architecturally acceptable. An alternative would be `block_sensitivity(const std::vector<double>& medians)` accepting raw medians, but the current design keeps the call site simpler and the coupling is narrow (only `CandidateRecord` is used).

3. **minor** — `coalescing_sensitivity` from §15 записки is not covered in this task. If intentionally deferred (it requires strided-vs-linear access pattern comparison, not just a Runtime API query), this should be noted in the packet or implementation notes as a conscious scoping decision. Not blocking: the metric arguably belongs in a testing/benchmark methodology, not in the `analysis` module.

4. **minor** — `scaling_exponent()` computes `std::log()` twice per data point (accumulation loop + regression loop). Negligible for the expected input size (~3-10 points), but a single-pass approach caching log values would be cleaner. Not worth changing.

5. **minor** — No explicit test for `scaling_exponent` with negative time value. The `time_ms <= 0.0` validation path is covered indirectly (zero-N test hits a different guard first), but an explicit `{100, -1.0}` test case would strengthen the contract.

### Design

No findings. Key decisions are sound:

- `build_fingerprint` decoupled from `BenchmarkResult`/`AutoTuneResult` — takes `ProfilingBreakdown` + `RunStats` + optional metric structs. Clean for diagnostics-advisor integration.
- `suggest_block_size` as template wrapper — correct for `cudaOccupancyMaxPotentialBlockSize` C++ API.
- `ScopedDevice` RAII in `detail/runtime_api.hpp` — proper save/restore, non-copyable.
- `estimate_bandwidth` uses `cudaDeviceGetAttribute` instead of `cudaDeviceProp` fields — justified deviation for CUDA 13.0 compatibility.
- `utilization_ratio` clamped via `std::clamp(0.0, 1.0)` — correct for heuristic metric.

## 5. Test Coverage Assessment

| Area | Host-only | CUDA | Negative |
|------|-----------|------|----------|
| `transfer_compute_ratio` | Yes | — | Yes (kernel_ms <= 0) |
| `block_sensitivity` | Yes (3 scenarios) | — | Yes (empty) |
| `scaling_exponent` | Yes (linear, quadratic, constant) | — | Yes (< 2 points, zero N) |
| `estimate_bandwidth` | — | Yes (known values, zero transfer) | Yes (zero time) |
| `estimate_occupancy` | — | Yes (normal, small block, max block) | Yes (null, invalid block) |
| `suggest_block_size` | — | Yes (warp alignment) | — |
| `get_kernel_attributes` | — | Yes (regs, limits) | Yes (null) |
| `query_memory_pressure` | — | Yes (ranges) | Yes (invalid device) |
| `KernelFingerprint` | Yes (construction) | — | — |

Coverage is sufficient for acceptance. Missing negative-time test for `scaling_exponent` is noted as minor #5.

## 6. Summary

| Category | Blockers | Major | Minor |
|----------|----------|-------|-------|
| Process | 0 | 1 | 0 |
| Code | 0 | 0 | 5 |
| Design | 0 | 0 | 0 |
| **Total** | **0** | **1** | **5** |

**Disposition: accepted.** The major process finding (uncommitted changes) must be resolved before merge to `main`, but does not block acceptance of the implementation itself. All minor code findings are non-blocking observations.

## Disposition Update

- Resolved in current branch:
  - process finding closed by recording the task through staged commits (`spec:` → `test:` → `impl:` → `review:`)
  - minor #1 closed: `block_sensitivity()` now initializes `max_median` with `-infinity`
  - minor #3 closed: `coalescing_sensitivity` is now explicitly documented as deferred/out of scope in the packet and implementation notes
  - minor #5 closed: added an explicit negative-time test for `scaling_exponent()`
- Intentionally left as-is:
  - minor #2 (`analysis` depending on `autotune::CandidateRecord`) — accepted narrow Layer 2 coupling
  - minor #4 (`std::log()` computed twice) — not worth complicating the implementation for the expected input size

# Review Report

## Task

- Name: Post-MVP Phase 6 - Diagnostics Advisor
- Branch: `task/diagnostics-advisor`
- Reviewer: Claude
- Review surface: `git diff 30f8b47...0c05bfa`
- Date: 2026-04-05

## Gate Compliance

| Gate | Status | Notes |
| --- | --- | --- |
| Spec (`00-feature-packet`) | Pass | Packet defines scope, rule set, NVIDIA references, risks, and acceptance criteria |
| Tests (`01-test-contract`) | Pass | Contract covers all rule paths, boundaries, negative scenarios, pipeline integration, and reporting |
| Implementation (`02-implementation-notes`) | Pass | Notes record deviations, validation commands, and follow-ups |
| Stage order | Pass | Commit sequence preserved: `9657b63 -> d35a0c2 -> 0c05bfa` |

## Vision Alignment

Task 6 addresses P4 diagnostics from the post-MVP vision. The delivered shape matches the
intended UX: a rule-based recommendation layer built on `KernelFingerprint`, exposed through
`Pipeline::diagnose()`, and propagated into `PipelineReport` for later exporters.

This task correctly builds on task 4 (`analysis-expansion`), which introduced the runtime
metrics and derived fields the advisor consumes.

## Findings

### Blockers

None.

### Major

**M1. Duplicated launch-config comparison and winner lookup logic.**

The original task diff duplicated the same launch-config comparison and winning-candidate
lookup logic across `pipeline.hpp` and `export.hpp`. That created a maintenance risk if
`KernelLaunchConfig` or autotune selection rules changed.

Reviewer recommendation:

- Centralize launch-config equality in a shared location.
- Centralize winning-candidate lookup in a shared location.
- Make both `pipeline.hpp` and `export.hpp` delegate to the shared helpers.

### Minor

**m1. `well_utilized` used defensive `>= 0.0` guards without explaining intent.**

The guards were correct but visually noisy. Reviewer recommendation: either remove them or
document that they are intentionally defensive against incomplete or corrupted metrics.

**m2. `Recommendation::severity` is a `std::string` instead of an enum.**

This remains a low-priority type-safety observation. The current rule set is small and fully
tested, so the issue is not blocking.

**m3. `detail::add_recommendation` adds limited value.**

This is a readability/style observation only.

**m4. `Pipeline::diagnose()` silently leaves `fingerprint` empty when `kernel_ms <= 0.0`.**

Behavior is correct, but the API does not explicitly signal that diagnostics were skipped.
This remains low priority.

## Test Coverage Assessment

| Contract scenario | Test file | Covered |
| --- | --- | --- |
| Healthy fingerprint -> empty | `advisor_test.cpp:39` | Yes |
| `transfer_dominated` | `advisor_test.cpp:43` | Yes |
| `low_occupancy` + guard | `advisor_test.cpp:54` | Yes |
| `local_memory_pressure` + guard | `advisor_test.cpp:64` | Yes |
| `bandwidth_bound` | `advisor_test.cpp:73` | Yes |
| `unstable_timing` | `advisor_test.cpp:83` | Yes |
| `block_sensitive` | `advisor_test.cpp:93` | Yes |
| `superlinear_scaling` + guard | `advisor_test.cpp:99` | Yes |
| `well_utilized` + guard | `advisor_test.cpp:108` | Yes |
| Multiple findings | `advisor_test.cpp:121` | Yes |
| Non-empty fields | `advisor_test.cpp:134` | Yes |
| Boundary `tcr == 3.0` / `3.01` | `advisor_test.cpp:146` | Yes |
| Negative metrics | `advisor_test.cpp:155` | Yes |
| All-zero fingerprint | `advisor_test.cpp:155` | Yes |
| Pipeline benchmark-only diagnose (CUDA) | `pipeline_diagnose_test.cu:94` | Yes |
| Pipeline autotune-only diagnose (CUDA) | `pipeline_diagnose_test.cu:109` | Yes |
| Pipeline diagnose export (CUDA) | `pipeline_diagnose_test.cu:125` | Yes |
| Pipeline diagnose without timing | `pipeline_test.cpp:58` | Yes |
| Pipeline JSON with diagnostics | `export_test.cpp:219` | Yes |
| Pipeline CSV with diagnostics | `export_test.cpp:186` | Yes |

All acceptance criteria from the feature packet are covered, including the host-only
no-timing path through `PipelineSuiteTest.DiagnoseWithoutTimingDataLeavesFingerprintEmpty`.

## Code Quality

- Header-only change set, consistent with the rest of the library
- No new dependencies
- Rule logic remains flat and auditable against the packet
- Pipeline/reporting integration stays minimal and low-risk
- Advice text remains grounded in NVIDIA guidance captured in the packet

## Lead Disposition

Review follow-up completed on 2026-04-05.

- **M1 addressed.** Launch-config equality is now centralized via `operator==` on
  `KernelLaunchConfig` in `include/cuda_test/core/types.hpp`, and winner lookup is shared via
  `autotune::detail::find_winning_candidate_index()` in
  `include/cuda_test/autotune/search.hpp`. `pipeline.hpp` and `export.hpp` now delegate to the
  shared logic.
- **m1 addressed.** `advisor.hpp` now documents why the defensive non-negative guards are kept
  in the `well_utilized` rule.
- **m2-m4 remain non-blocking observations.** They do not require changes for this task gate.

Validation after the fixes:

- `msvc`: `AdvisorTest.*`, `PipelineSuiteTest.*`, `ExportTest.*`
- `msvc-cuda`: `AdvisorTest.*`, `PipelineDiagnoseCudaTest.*`

Result: accepted after review follow-up.

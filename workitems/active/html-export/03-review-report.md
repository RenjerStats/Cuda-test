# Review Report

## Task

- Name: Post-MVP Phase 7 - HTML Export
- Branch: `task/html-export`
- Reviewer: Claude
- Review surface: `git diff 21c7b10...47048da`
- Date: 2026-04-05

## Gate Compliance

| Gate | Status | Notes |
| --- | --- | --- |
| Spec (`00-feature-packet`) | Pass | Packet defines scope, rendering semantics, visual semantics, risks, and acceptance criteria |
| Tests (`01-test-contract`) | Pass | Contract covers full report, minimal report, escaping, delegation, suite, empty suite, parent creation, invalid path, script-sensitive text |
| Implementation (`02-implementation-notes`) | Pass | Notes record deviations (include-cycle workaround, no browser automation), validation commands, and build results |
| Stage order | Pass | Commit sequence preserved: `2d0e04a -> 3e6e405 -> 47048da` |

## Vision Alignment

Task 7 addresses P6 (lack of visual reports) from the post-MVP vision. The delivered shape
matches the intended UX: `report.to_html("result.html")` produces a self-contained HTML
document with inline CSS, inline SVG charts, and embedded raw JSON.

Dependencies on task 2 (pipeline-suite) and task 6 (diagnostics-advisor) are satisfied.
The task correctly consumes `PipelineReport` and `SuiteReport` types from Layer 3 and
renders all pipeline stages (correctness, benchmark, autotune, diagnostics) when present.

## Findings

### Blockers

None.

### Major

**M1. Duplicated JSON serialization between `html_export.hpp` and `export.hpp`.**

`html_export.hpp` contains a full parallel set of JSON-writing functions (`html_write_run_stats_json`,
`html_write_launch_config_json`, `html_write_pipeline_report_json`, etc.) that are near-identical
copies of the corresponding functions in `export.hpp` (`write_run_stats_json`,
`write_launch_config_json`, `write_pipeline_report_json`).

The implementation notes explain this as an include-cycle workaround, but both headers already
include `pipeline_report.hpp` and live in the same `reporting` module. The JSON-writing helpers
could be extracted into a shared `reporting/detail/json_writer.hpp` that both include.

Risk: when `PipelineReport` or related types gain new fields, the JSON logic must be updated in
two places or the embedded JSON in HTML reports silently diverges from the standalone JSON export.

Disposition: accepted as tech-debt. Should be resolved in the next reporting-related task.

**M2. `#define private public` in test ([html_export_test.cpp:12](tests/unit/reporting/html_export_test.cpp#L12)).**

The test uses `#define private public` to construct `PipelineReport` and `SuiteReport` with
specific field values. This is undefined behavior per the C++ standard (ODR violation) and
fragile across compilers and link configurations.

This is a systemic issue — earlier tests (`export_test.cpp`, `pipeline_test.cpp`) use the same
pattern. New code should not extend it further.

Disposition: accepted as systemic tech-debt. A future task should introduce test builders or
`friend` declarations for test construction.

### Minor

**m1. Monolithic 1250-line header.** CSS, SVG generation, JSON serialization, and DOM rendering
are all in one file. Splitting into `detail/html_css.hpp`, `detail/html_svg.hpp`, and
`detail/html_sections.hpp` would improve navigability without changing the public API.

**m2. `format_percent` produces excessive precision.** `html_format_double` uses `setprecision(17)`,
so percentages render as e.g. `68.000000000000003%`. For a visual HTML report, 1-2 decimal places
would be more appropriate.

**m3. Stacked bar chart corner radius on edge segments.** The first segment has `rx="14"` and
the last has `rx="14"`, but the middle segment has none. When edge segments are very narrow
(small H2D or D2H share), the corner radius exceeds the segment width, producing visual artifacts.

**m4. Theoretical timestamp flakiness.** `PipelineMemberDelegationMatchesDirectExport` compares
two export outputs for text equality. Both calls invoke `generated_timestamp()` from
`std::time()`. If the calls straddle a second boundary, the test fails. Extremely unlikely but
technically flaky.

## Acceptance Criteria Check

| Criterion | Status |
| --- | --- |
| `export_html(path, PipelineReport)` writes readable HTML | Pass |
| `export_html(path, SuiteReport)` writes readable HTML | Pass |
| `PipelineReport::to_html()` delegates to `reporting::export_html` | Pass |
| `SuiteReport::to_html()` delegates to `reporting::export_html` | Pass |
| HTML starts with `<!DOCTYPE html>` | Pass |
| Pipeline HTML contains kernel name in title and header | Pass |
| Correctness state shown when enabled | Pass |
| Candidates table and SVG chart when autotune data exists | Pass |
| Breakdown SVG when timing data exists | Pass |
| Recommendations with severity styling when diagnostics enabled | Pass |
| Empty-state text when optional sections absent | Pass |
| Suite HTML contains summary table and per-kernel sections | Pass |
| Raw JSON in `<script type="application/json">` | Pass |
| Visible special characters HTML-escaped | Pass |
| No external `http://` or `https://` links | Pass |
| Host-only unit tests cover contract scenarios | Pass |

## Verdict

**Accepted** with two major findings recorded as tech-debt (M1, M2). Neither blocks
correctness or usability of the delivered feature. All acceptance criteria are met.

## Lead Disposition

Follow-up changes were applied on top of the reviewed `impl` commit:

- `M1` fixed by extracting shared report JSON serialization into
  `include/cuda_test/reporting/detail/json_writer.hpp` and reusing it from both
  `include/cuda_test/reporting/export.hpp` and `include/cuda_test/reporting/html_export.hpp`
- `M2` fixed for this task by adding `pipeline::detail::PipelineReportBuilder` and
  `pipeline::detail::SuiteReportBuilder` in
  `include/cuda_test/pipeline/pipeline_report.hpp`, then rewriting
  `tests/unit/reporting/html_export_test.cpp` to stop using private-access macro tricks
- `m2` fixed by rendering percentages with one decimal place in the HTML view
- `m3` fixed by clamping stacked-bar corner radius to the segment width/height
- `m4` fixed by normalizing generated timestamps in direct-vs-member delegation tests

Validation rerun after the follow-up:

- `cmake --build --preset msvc --config Debug --target reporting_html_export_test reporting_export_test pipeline_suite_test`
- `cmake --build --preset msvc-cuda --config Debug --target reporting_html_export_test reporting_export_test pipeline_suite_test`
- `ctest -C Debug --output-on-failure -R '^(HtmlExportTest\\.|ExportTest\\.|PipelineSuiteTest\\.)'` in `build/msvc`
- `ctest -C Debug --output-on-failure -R '^(HtmlExportTest\\.|ExportTest\\.|PipelineSuiteTest\\.)'` in `build/msvc-cuda`

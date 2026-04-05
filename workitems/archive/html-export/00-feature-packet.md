# Feature Packet

## Task

- Name: Post-MVP Phase 7 - HTML Export
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/html-export`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` - resolves P6

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: section 10 (report formats), section 19 (experimental tables)
- Related section(s) in vision: `P6 - lack of visual reports`
- Upstream task dependencies:
  - `workitems/active/export-refactor/`
  - `workitems/active/diagnostics-advisor/`

## Objective

Add self-contained HTML export for `PipelineReport` and `SuiteReport` so the library can
produce a browser-readable report without external post-processing scripts.

The HTML output must stay static and dependency-free:

- inline CSS only
- inline SVG charts only
- no external JavaScript
- no external CSS
- embedded raw JSON for reuse

## Scope

- In scope:
  - New public header `include/cuda_test/reporting/html_export.hpp`
  - `reporting::export_html(path, const pipeline::PipelineReport&)`
  - `reporting::export_html(path, const pipeline::SuiteReport&)`
  - `PipelineReport::to_html(path)`
  - `SuiteReport::to_html(path)`
  - Self-contained HTML with inline CSS and inline SVG
  - Pipeline report sections:
    - title + hero header with kernel name, stage status, device label, generated timestamp, library version
    - correctness badge
    - summary cards for available benchmark/autotune/diagnostics state
    - autotune candidate table with winner highlight when autotune data exists
    - autotune median bar chart as inline SVG when candidate data exists
    - breakdown stacked bar for H2D / kernel / D2H using benchmark data or winning autotune candidate
    - timing table for benchmark data or winning autotune candidate
    - recommendation cards with severity-specific styling
    - embedded raw JSON in `<script type="application/json">`
  - Suite report sections:
    - suite hero header
    - summary table across kernels
    - one inline section per kernel report
    - embedded suite-level raw JSON
  - HTML escaping for all user-controlled visible text and JSON-safe script embedding
  - Host-only unit tests for structure and key content

- Out of scope:
  - interactive UI, sorting, filtering, or chart animation
  - PDF export
  - user-customizable themes/templates
  - external assets
  - standalone `export_json(path, SuiteReport)` or `export_csv(path, SuiteReport)`

## Affected Areas

- Modules: `reporting`
- Public headers:
  - `include/cuda_test/reporting/html_export.hpp` (new)
  - `include/cuda_test/pipeline/pipeline_report.hpp` (new `to_html`)
  - `include/cuda_test/cuda_test.hpp` (umbrella include update if needed)
- Related headers:
  - `include/cuda_test/reporting/export.hpp` (shared JSON writer helpers)
  - `include/cuda_test/core/version.hpp` (new shared version constants for header display)
- Tests:
  - `tests/unit/reporting/html_export_test.cpp`

## Interface Notes

### New Public API

```cpp
namespace cuda_test::reporting {

void export_html(const std::filesystem::path& path,
                 const pipeline::PipelineReport& report);

void export_html(const std::filesystem::path& path,
                 const pipeline::SuiteReport& report);

} // namespace cuda_test::reporting
```

```cpp
namespace cuda_test::pipeline {

class PipelineReport {
public:
    void to_html(const std::filesystem::path& path) const;
};

class SuiteReport {
public:
    void to_html(const std::filesystem::path& path) const;
};

} // namespace cuda_test::pipeline
```

### Rendering Semantics

- `PipelineReport` writes one HTML file for one kernel report.
- `SuiteReport` writes one HTML file for the whole suite, not a directory.
- Parent directories are created automatically, matching existing export behavior.
- HTML stays valid and useful even when the report is mostly empty.
- Device info is best-effort:
  - if runtime device metadata is available, show device name and compute capability
  - otherwise show `Device <id>`
- The raw JSON `<script>` must stay safe when report strings contain `<`, `>`, `&`, or `</script>`.

### Visual Semantics

- Winner candidate rows use a distinct class and accent color.
- Recommendation severity mapping:
  - `critical` -> red
  - `warning` -> amber
  - `info` -> blue
- Empty states are explicit:
  - `No timing data available`
  - `No recommendations generated`
  - `No kernels tested`

## Risks

- String-built HTML can become hard to maintain.
  - Mitigation: keep rendering split into small helpers per section.
- Script embedding can be broken by raw `<script>` terminators inside JSON strings.
  - Mitigation: escape `<`, `>`, and `&` in embedded JSON text.
- Host-only environments cannot always provide CUDA device metadata.
  - Mitigation: degrade gracefully to `Device <id>` instead of throwing.
- Large autotune candidate sets can make charts too tall.
  - Mitigation: render top 10 candidates by median kernel time and note truncation.

## Acceptance Criteria

- [ ] `export_html(path, PipelineReport)` writes a readable HTML file at the requested path
- [ ] `export_html(path, SuiteReport)` writes a readable HTML file at the requested path
- [ ] `PipelineReport::to_html()` delegates to `reporting::export_html`
- [ ] `SuiteReport::to_html()` delegates to `reporting::export_html`
- [ ] HTML starts with `<!DOCTYPE html>`
- [ ] Pipeline HTML contains kernel name in both `<title>` and visible header
- [ ] Pipeline HTML contains correctness state when correctness is enabled
- [ ] Pipeline HTML contains a candidates table and SVG chart when autotune data exists
- [ ] Pipeline HTML contains a breakdown SVG when timing data exists
- [ ] Pipeline HTML contains a recommendations section with severity styling when diagnostics are enabled
- [ ] Pipeline HTML contains explicit empty-state text when optional sections are absent
- [ ] Suite HTML contains a summary table and one section per kernel report
- [ ] Embedded raw JSON is present in `<script type="application/json">`
- [ ] Visible special characters are HTML-escaped
- [ ] HTML contains no `http://` or `https://` external asset links
- [ ] Host-only unit tests cover the structural scenarios from the contract

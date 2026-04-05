# Implementation Notes

## Summary

Implemented self-contained HTML export for `PipelineReport` and `SuiteReport`:

- added `reporting::export_html(path, const pipeline::PipelineReport&)`
- added `reporting::export_html(path, const pipeline::SuiteReport&)`
- added `PipelineReport::to_html(path)`
- added `SuiteReport::to_html(path)`
- added new public header `include/cuda_test/reporting/html_export.hpp`

The renderer produces:

- inline CSS only
- inline SVG charts for autotune candidates and timing breakdown
- inline recommendations cards
- inline `application/json` payload for raw report reuse

## Notable Decisions

### 1. `SuiteReport::to_html(path)` writes one file, not a directory

This follows the task packet and differs intentionally from `to_json(dir)` / `to_csv(dir)`.
The HTML suite report is a single browser document with:

- suite summary table
- one inline section per kernel report

### 2. Added `core/version.hpp`

The HTML header needs a stable library version string. The version constants previously lived
only in `cuda_test.hpp`, so they were moved into `include/cuda_test/core/version.hpp` and
re-exported through the umbrella header without changing the public namespace
(`cuda_test::detail::version_*` remains intact).

### 3. HTML renderer is self-contained

The first implementation attempt reused `reporting/export.hpp` internals for formatting and
JSON embedding. That created an include-cycle problem because `pipeline_report.hpp` includes
both `export.hpp` and `html_export.hpp`.

Final approach:

- keep `export.hpp` as the JSON/CSV export surface
- keep `html_export.hpp` self-contained for file opening, numeric formatting, and JSON embedding
- still reuse the same public report types and autotune winner-selection logic

This trades a small amount of duplication for simpler header wiring and deterministic builds.

### 4. Test temp paths are build-specific

Both the new HTML tests and the pre-existing `pipeline_test.cpp` used shared temp-directory
names. Because validation was run against `msvc` and `msvc-cuda` in parallel, those tests
could race on the same filesystem path.

The temp roots now include a hash of `std::filesystem::current_path()` so host-only and CUDA
builds do not collide.

## Deviations From The Packet

- The packet mentioned best-effort device metadata in the report header. The implementation
  shows full device info only when CUDA runtime metadata is available; otherwise it falls back
  to `Device <id>` without throwing.
- The packet suggested reuse of export internals for embedded JSON. This was not retained due
  the header include-cycle described above.
- No browser automation was added. Structural validation stays in unit tests.

## Validation

Configured:

- `cmake --preset msvc`
- `cmake --preset msvc-cuda`

Built:

- `cmake --build --preset msvc --config Debug --target reporting_html_export_test reporting_export_test pipeline_suite_test`
- `cmake --build --preset msvc-cuda --config Debug --target reporting_html_export_test reporting_export_test pipeline_suite_test`

Tested:

- `ctest -C Debug --output-on-failure -R '^(HtmlExportTest\\.|ExportTest\\.|PipelineSuiteTest\\.)'` in `build/msvc`
- `ctest -C Debug --output-on-failure -R '^(HtmlExportTest\\.|ExportTest\\.|PipelineSuiteTest\\.)'` in `build/msvc-cuda`

Result:

- host-only targeted suite passed
- CUDA preset targeted suite passed

Manual browser-open smoke check:

- not performed in this CLI session

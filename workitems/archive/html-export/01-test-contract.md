# Test Contract

## Linkage

- Related feature packet: `workitems/active/html-export/00-feature-packet.md`
- Reviewer: Claude

## Correctness Scenarios

| Scenario | Input shape | Expected result | Validation method |
| --- | --- | --- | --- |
| Pipeline HTML - full report | report with correctness, benchmark, autotune, diagnostics | file created and non-trivial in size | existence + size |
| Pipeline HTML - doctype | any pipeline report | file starts with `<!DOCTYPE html>` | prefix check |
| Pipeline HTML - title contains kernel name | `kernel_name="test_kernel"` | `<title>` contains the name | substring |
| Pipeline HTML - visible header contains kernel name | same | visible header contains the name | substring |
| Pipeline HTML - correctness passed badge | correctness enabled + passed | contains `Passed` status text | substring |
| Pipeline HTML - correctness failed badge | correctness enabled + failed | contains `Failed` status text | substring |
| Pipeline HTML - winner highlight | autotune result with clear winner | HTML contains winner class marker | substring |
| Pipeline HTML - candidate table | autotune result with 3 candidates | table exists with candidate rows | substring count |
| Pipeline HTML - candidate bar chart | autotune result with candidates | contains `<svg` and candidate bars | substring |
| Pipeline HTML - breakdown chart | benchmark or autotune winner timing available | contains breakdown section and SVG rectangles | substring |
| Pipeline HTML - benchmark table | benchmark result present | contains timing table headers and stage rows | substring |
| Pipeline HTML - recommendations section | diagnose enabled with 2 recommendations | contains summaries and severity classes | substring |
| Pipeline HTML - empty recommendations | diagnose enabled and no recommendations | contains `No recommendations generated` | substring |
| Pipeline HTML - raw JSON embedded | any report | contains `<script type="application/json"` and report JSON fields | substring |
| Pipeline HTML - special chars escaped | kernel name contains `<`, `>`, `&` | visible text is escaped | substring |
| Pipeline HTML - no external links | any report | no `http://` or `https://` | absence check |
| Pipeline HTML - minimal empty report | no correctness, no benchmark, no autotune, no diagnose | still valid HTML with explicit no-data state | substring |
| Pipeline HTML - member delegation | same report exported both ways | direct and member output match | file text equality |
| Suite HTML - empty suite | `reports().empty()` | valid HTML with `No kernels tested` | substring |
| Suite HTML - summary table | suite with 2 reports | summary table contains both kernel names | substring |
| Suite HTML - per-kernel sections | suite with 2 reports | contains 2 kernel sections/articles | substring count |
| Suite HTML - raw JSON embedded | suite with reports | script block contains suite name and report array fields | substring |
| Suite HTML - member delegation | same suite exported both ways | direct and member output match | file text equality |
| Parent directory creation | output path parent does not exist | file written successfully | existence |

## Negative Scenarios

| Scenario | Failure trigger | Expected behavior |
| --- | --- | --- |
| Invalid path | output path points to a directory | throws exception | exception assertion |
| Script-sensitive text | strings contain `</script>` | HTML stays intact and script block remains embedded | substring absence/presence |

## Numerical Policy

- No floating-point tolerances beyond textual presence checks
- Timing values are verified structurally, not against a browser renderer

## Performance Scenarios

- Not applicable

## Evidence Required

- Host-only unit tests: `tests/unit/reporting/html_export_test.cpp`
- Targeted regression run:
  - `reporting_export_test`
  - `reporting_html_export_test`
  - `pipeline_suite_test`
- Manual browser-open smoke check recorded in implementation notes

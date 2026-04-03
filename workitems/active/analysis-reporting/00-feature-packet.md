# Feature Packet

## Task

- Name: Phase 6 - Analysis + Reporting / analysis-reporting
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/analysis-reporting`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §5 (reporting as functional result), §6 (required metrics), §11 (analysis and reporting modules), §15 (`transfer_compute_ratio`), §18 (profiling report acceptance), §19 (report tables)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задачи 4, 5, 6; agreed stack with CSV/JSON reporting

## Objective

Реализовать минимальные модули `analysis` и `reporting`, которые опираются на результаты `benchmark` и `autotune` и готовят данные к последующим экспериментам. Результат фазы должен дать вычисление `transfer_compute_ratio` и детерминированный экспорт `AutoTuneResult` в CSV/JSON без привязки к конкретным CUDA-ядрам.

## Scope

- In scope:
  - `analysis::transfer_compute_ratio(const core::ProfilingBreakdown&)`
  - `reporting::export_csv(const std::filesystem::path&, const autotune::AutoTuneResult&)`
  - `reporting::export_json(const std::filesystem::path&, const autotune::AutoTuneResult&)`
  - deterministic CSV export for candidate summaries
  - deterministic JSON export for full structured autotune results
  - host-only unit tests for metric calculation and CSV/JSON file contents
  - umbrella-header integration for analysis/reporting public APIs

- Out of scope:
  - importing/parsing report files as public library API
  - experiment-matrix automation under `reports/`
  - figure generation or table post-processing
  - additional derived metrics such as `coalescing_sensitivity` and `scaling_exponent`
  - direct GPU experiments or accepted evidence under `reports/`

## Affected Areas

- Modules:
  - `analysis`
  - `reporting`
- Public headers:
  - `include/cuda_test/analysis/metrics.hpp`
  - `include/cuda_test/reporting/export.hpp`
- Internal components:
  - `tests/unit/analysis/`
  - `tests/unit/reporting/`
  - `tests/CMakeLists.txt`
  - `include/cuda_test/cuda_test.hpp`
- Reports or methodology:
  - output field names must stay aligned with `docs/methodology/benchmark-protocol.md`

## Interface Notes

- New or changed types:
  - no new top-level data structures beyond existing `AutoTuneResult`
- New or changed functions:
  - `double transfer_compute_ratio(const core::ProfilingBreakdown& breakdown)`
  - `void export_csv(const std::filesystem::path& path, const autotune::AutoTuneResult& result)`
  - `void export_json(const std::filesystem::path& path, const autotune::AutoTuneResult& result)`
- Input or output assumptions:
  - `transfer_compute_ratio` requires positive `kernel_ms`
  - export functions create parent directories when needed
  - CSV export may legally contain only a header row when `all_candidates` is empty
  - JSON export includes the winner summary plus the full candidate array

## Risks

- Technical risks:
  - manual JSON/CSV formatting can drift from later reporting needs; mitigation: lock a deterministic field set in tests now
  - `AutoTuneResult` currently exposes `RunStats` and candidate benchmark records but not external metadata like kernel name; mitigation: keep Phase 6 focused on serializing the existing result object only
- Measurement risks:
  - this phase makes no performance claims; it only serializes existing benchmark/autotune outputs
- Integration risks:
  - report schema changes later can break downstream scripts; mitigation: make field names explicit in packet, tests, and exported headers

## Acceptance Criteria

- [ ] `include/cuda_test/analysis/metrics.hpp` and `include/cuda_test/reporting/export.hpp` provide public APIs
- [ ] `transfer_compute_ratio` returns the expected ratio and rejects invalid `kernel_ms`
- [ ] CSV export writes a deterministic candidate table and handles empty candidate sets
- [ ] JSON export writes a deterministic structured summary for `AutoTuneResult`
- [ ] host-only analysis/reporting unit tests pass in `msvc` and `default`-style builds

# Implementation Notes

## Summary

Реализован минимальный `autotune`-слой для Phase 5: добавлены `AutoTuneSpec`, `CandidateRecord`, `AutoTuneResult` и `tune_kernel()` поверх `BenchmarkRunner`. Модуль генерирует 1D-кандидаты из `block_sizes` и `grid_wave_multipliers`, отфильтровывает oversized block sizes до измерений, ранжирует победителя по `median_ms -> p95_ms -> cv` и формирует человекочитаемую причину выбора. Для проверки логики поиска и tie-break правил добавлены host-only unit-тесты на synthetic candidate measurements.

## Files And Modules

- Files touched:
  - `include/cuda_test/autotune/search.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `tests/unit/autotune/autotune_search_test.cpp`
  - `tests/CMakeLists.txt`
  - `workitems/archive/autotune-search/00-feature-packet.md`
  - `workitems/archive/autotune-search/01-test-contract.md`
  - `workitems/archive/autotune-search/03-review-report.md`
- Modules touched:
  - `autotune`
  - umbrella public include surface
  - host-only unit-test registration

## Deviations From Packet

- No functional deviations.
- `autotune` implementation kept header-only for now, because Phase 5 only needs deterministic candidate generation and template-based orchestration on top of the existing `benchmark` API.

## Validation

- Commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc --target autotune_search_test cuda_test_tests --config Debug`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc -C Debug --output-on-failure -R "AutoTuneSearchTest|ComputeStatsTest|BenchmarkRunnerTest"`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc -C Debug --output-on-failure`
  - `VsDevCmd.bat + Ninja + cmake.exe --preset default`
  - `VsDevCmd.bat + Ninja + cmake.exe --build build/default --target autotune_search_test cuda_test_tests`
  - `VsDevCmd.bat + Ninja + ctest.exe --test-dir build/default --output-on-failure -R "AutoTuneSearchTest|ComputeStatsTest|BenchmarkRunnerTest"`
  - `VsDevCmd.bat + Ninja + ctest.exe --test-dir build/default --output-on-failure`
- Result summary:
  - `msvc` host-only configure/build succeeded; targeted autotune + benchmark subset passed `17/17`, and the full host-only suite passed `25/25`.
  - `default` host-only configure/build also succeeded in a `VsDevCmd + Ninja` environment; targeted autotune + benchmark subset passed `17/17`, and the full host-only suite passed `25/25`.
  - The new autotune public header integrated cleanly into `cuda_test.hpp` without breaking existing `core`, `benchmark`, or `testing` host-only tests.

## Follow-Ups

- Pending cleanup:
  - decide in a later phase whether `AutoTuneResult` should expose the full winning `BenchmarkResult` directly in addition to winner `RunStats`
- Deferred work:
  - baseline-vs-winner reporting
  - real CUDA autotune evidence for course kernels
  - richer candidate constraints such as occupancy heuristics or multi-dimensional launch search


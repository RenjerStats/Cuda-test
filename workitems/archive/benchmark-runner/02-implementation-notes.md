# Implementation Notes

## Summary

Реализован минимальный `benchmark`-слой для Phase 4: добавлены `compute_stats()` для расчёта `RunStats`, `BenchmarkRunner` с warm-up и measured iterations, а также агрегация `ProfilingBreakdown` в stage-specific статистику. Для проверки интеграции с Google Benchmark зарегистрирован host-only smoke target `benchmark_runner_smoke`, а для контроля формул и поведения runner добавлены unit-тесты в `tests/unit/benchmark/`.

## Files And Modules

- Files touched:
  - `include/cuda_test/benchmark/stats.hpp`
  - `include/cuda_test/benchmark/benchmark_runner.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `tests/unit/benchmark/benchmark_stats_test.cpp`
  - `tests/unit/benchmark/benchmark_runner_test.cpp`
  - `tests/CMakeLists.txt`
  - `benchmarks/CMakeLists.txt`
  - `benchmarks/benchmark_runner_smoke.cpp`
  - `workitems/active/benchmark-runner/00-feature-packet.md`
  - `workitems/active/benchmark-runner/01-test-contract.md`
  - `workitems/active/benchmark-runner/03-review-report.md`
- Modules touched:
  - `benchmark`
  - umbrella public include surface
  - benchmark and unit-test registration infrastructure

## Deviations From Packet

- No functional deviations.
- `benchmark` implementation kept header-only for now, because Phase 4 only needs reusable statistical helpers and a templated runner around callables returning `ProfilingBreakdown`.

## Validation

- Commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc --target benchmark_stats_test benchmark_runner_test benchmark_runner_smoke --config Debug`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc -C Debug --output-on-failure -R "ComputeStatsTest|BenchmarkRunnerTest"`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc --target cuda_test_tests --config Debug`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc -C Debug --output-on-failure`
  - `build/msvc/benchmarks/Debug/benchmark_runner_smoke.exe --benchmark_list_tests`
  - `VsDevCmd.bat + Ninja + cmake.exe --fresh --preset default`
  - `VsDevCmd.bat + Ninja + cmake.exe --build build/default --target benchmark_stats_test benchmark_runner_test benchmark_runner_smoke cuda_test_tests`
  - `VsDevCmd.bat + Ninja + ctest.exe --test-dir build/default --output-on-failure -R "ComputeStatsTest|BenchmarkRunnerTest"`
  - `VsDevCmd.bat + Ninja + ctest.exe --test-dir build/default --output-on-failure`
  - `build/default/benchmarks/benchmark_runner_smoke.exe --benchmark_list_tests`
- Result summary:
  - `msvc` configure succeeded with Google Test and Google Benchmark dependencies resolved via `FetchContent`.
  - New benchmark targets built successfully: `benchmark_stats_test`, `benchmark_runner_test`, `benchmark_runner_smoke`.
  - `9/9` selected benchmark tests passed; full host-only suite passed `17/17`.
  - `benchmark_runner_smoke.exe --benchmark_list_tests` reported the registered benchmark `benchmark_runner_synthetic`.
  - `default` preset was validated successfully in a `VsDevCmd + Ninja` environment; the benchmark-only subset passed `9/9`, and the full host-only suite passed `17/17`.

## Follow-Ups

- Pending cleanup:
  - decide in a later phase whether `compute_stats()` should expose lower-level helpers for percentile and CI formulas or keep them internal
- Deferred work:
  - real CUDA benchmark fixtures for course kernels
  - report export for benchmark/autotune evidence
  - autotune candidate ranking on top of `BenchmarkRunner`

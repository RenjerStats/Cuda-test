# Implementation Notes

## Summary

Реализован Phase 7 integration path для шести representative CUDA kernels, соответствующих классам операций из `plan/Пояснительная записка.md` §16: `density_update`, `physics_integration`, `contact_flag`, `active_compaction`, `buffer_generation`, `interval_intersection`. Для них добавлены детерминированные host fixtures, CUDA integration tests и один runner, который прогоняет autotune на доступной GPU и экспортирует CSV/JSON evidence в `reports/tables/single-gpu/`.

Фаза закрывает runnable path через уже принятые модули `testing`, `profiling`, `benchmark`, `autotune`, `analysis` и `reporting`, не расширяя публичный API библиотеки. Evidence в текущей среде собран на одном доступном устройстве `NVIDIA GeForce RTX 4070 Laptop GPU`; второй GPU в ноутбуке отсутствует, поэтому dual-GPU прогон зафиксирован как deferred hardware constraint.

## Files And Modules

- Files touched:
  - `examples/kernels/kernel_suite.hpp`
  - `tests/fixtures/kernel_suite_fixture.hpp`
  - `tests/integration/kernel_integration_test.cu`
  - `benchmarks/kernels/kernel_integration_runner.cu`
  - `tests/CMakeLists.txt`
  - `benchmarks/CMakeLists.txt`
  - `workitems/active/kernel-integration/00-feature-packet.md`
  - `workitems/active/kernel-integration/01-test-contract.md`
  - `workitems/active/kernel-integration/03-review-report.md`
- Modules touched:
  - integration path across `testing`, `profiling`, `benchmark`, `autotune`, `analysis`, `reporting`
  - internal examples / fixtures / integration-runner layer
  - CUDA-only test and benchmark registration

## Deviations From Packet

- No scope deviation from the approved packet.
- The approved packet explicitly allowed bundled representative kernels instead of the actual external course-project sources, because that repository is not present in the current workspace.
- `active_compaction` validation compares sorted compacted indices rather than raw device order, because `atomicAdd` guarantees count correctness but does not guarantee a stable write order across blocks.

## Validation

- Environment:
  - visible CUDA device: `NVIDIA GeForce RTX 4070 Laptop GPU`
  - second discrete GPU: unavailable in the current laptop environment
- Commands run:
  - `cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --preset msvc-cuda'`
  - `cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build/msvc-cuda --target kernel_integration_test kernel_integration_runner --config Debug'`
  - `cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build/msvc-cuda -C Debug --output-on-failure -R "KernelIntegrationTest"'`
  - `cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && build\msvc-cuda\benchmarks\Debug\kernel_integration_runner.exe'`
  - `cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build/msvc-cuda --target cuda_test_tests --config Debug'`
  - `cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build/msvc-cuda -C Debug --output-on-failure'`
- Result summary:
  - selected CUDA integration subset passed `4/4`
  - full `msvc-cuda` test suite passed `51/51`
  - `kernel_integration_runner.exe` completed successfully and generated:
    - six per-kernel CSV reports
    - six per-kernel JSON reports
    - `reports/tables/single-gpu/kernel-suite-summary.csv`
    - `reports/tables/single-gpu/hardware-note.txt`
  - summary report records baseline vs best configuration, kernel median, total median, `transfer_compute_ratio`, and single-GPU hardware scope for all six kernels

## Follow-Ups

- Pending cleanup:
  - none inside Phase 7 scope
- Deferred work:
  - repeat the same report-generation flow on a second NVIDIA GPU when such hardware becomes available
  - replace bundled representative kernels with real course-project kernels once that external codebase is connected to this repository

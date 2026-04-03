# Implementation Notes

## Summary

Реализован `profiling`-модуль Phase 2: добавлен `StagedTimer` на CUDA Events с измерением этапов `h2d`, `kernel`, `d2h`, `total`, reset/reuse semantics и интеграцией в публичный umbrella header. Для проверки добавлен CUDA unit-тест на тривиальном kernel и исправлена политика warning flags, чтобы `.cu` targets корректно собирались под MSVC + nvcc.

## Files And Modules

- Files touched:
  - `include/cuda_test/profiling/staged_timer.hpp`
  - `include/cuda_test/core/detail/cuda_compat.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `tests/CMakeLists.txt`
  - `tests/unit/profiling/staged_timer_test.cu`
  - `cmake/Warnings.cmake`
  - `workitems/active/profiling-timer/00-feature-packet.md`
  - `workitems/active/profiling-timer/01-test-contract.md`
- Modules touched:
  - `profiling`
  - test registration infrastructure
  - shared warning configuration for CUDA builds

## Deviations From Packet

- `StagedTimer` kept header-only instead of moving logic into `src/profiling/`.
  Это сделано осознанно: текущее поведение целиком завязано на лёгкий RAII вокруг CUDA Events и не требует отдельной единицы компоновки. Если последующие фазы добавят тяжёлую логику, модуль можно вынести в compiled target без изменения публичного API.

## Validation

- Commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc-cuda`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc --target core_types_test`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc-cuda --target profiling_staged_timer_test`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc -C Debug --output-on-failure -R "KernelLaunchConfigTest|ProfilingBreakdownTest|RunStatsTest"`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc-cuda -C Debug --output-on-failure -R "StagedTimerTest|KernelLaunchConfigTest|ProfilingBreakdownTest|RunStatsTest|CheckCudaTest|DeviceMemoryTest|DeviceInfoTest"`
- Result summary:
  - `msvc` configure/build remained valid after introducing profiling public headers.
  - `msvc-cuda` configure/build succeeded.
  - `16/16` selected tests passed in `msvc-cuda`, including all profiling tests.

## Follow-Ups

- Pending cleanup:
  - consider whether `StagedTimer` should later expose explicit stream ownership or remain a passive recorder
- Deferred work:
  - integrate `StagedTimer` into benchmark/autotune runners
  - decide whether CUDA-specific warning suppression should be narrowed further for nvcc + gtest builds

# Implementation Notes

## Summary

Реализован базовый `core` слой для Phase 1: добавлены типы конфигурации и статистики, обёртка `DeviceMemory<T>`, обработка ошибок CUDA и helper для опроса устройств. Решение сделано header-first с условной совместимостью для host-only сборки, чтобы `msvc` и `msvc-cuda` пресеты оставались рабочими.

## Files And Modules

- Files touched:
  - `include/cuda_test/core/detail/cuda_compat.hpp`
  - `include/cuda_test/core/types.hpp`
  - `include/cuda_test/core/error.hpp`
  - `include/cuda_test/core/device_info.hpp`
  - `include/cuda_test/core/device_memory.hpp`
  - `include/cuda_test/cuda_test.hpp`
  - `src/CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `tests/unit/core/core_types_test.cpp`
  - `tests/unit/core/core_cuda_runtime_test.cpp`
  - `workitems/archive/core-types/00-feature-packet.md`
  - `workitems/archive/core-types/01-test-contract.md`
- Modules touched:
  - `core`
  - test registration infrastructure

## Deviations From Packet

- No functional deviations.
- Added `include/cuda_test/core/detail/cuda_compat.hpp` as a compatibility helper so `dim3` and CUDA error types do not break host-only builds.

## Validation

- Commands run:
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --preset msvc-cuda`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc --target core_types_test`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc-cuda --target core_types_test`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build/msvc-cuda --target core_cuda_runtime_test`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc -C Debug --output-on-failure -R "KernelLaunchConfigTest|ProfilingBreakdownTest|RunStatsTest"`
  - `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe --test-dir build/msvc-cuda -C Debug --output-on-failure -R "KernelLaunchConfigTest|ProfilingBreakdownTest|RunStatsTest|CheckCudaTest|DeviceMemoryTest|DeviceInfoTest"`
- Result summary:
  - `msvc` configure/build succeeded; 3/3 core type tests passed.
  - `msvc-cuda` configure/build succeeded; 10/10 core tests passed on the local RTX 4070 Laptop GPU environment.

## Follow-Ups

- Pending cleanup:
  - review whether `core` should stay header-only after later phases add heavier logic
- Deferred work:
  - richer device selection policy
  - stream-aware memory helpers
  - integration with profiling and benchmark modules from later phases


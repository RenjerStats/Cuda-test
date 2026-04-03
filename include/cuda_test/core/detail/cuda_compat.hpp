#pragma once

#include <cstddef>

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
// Public profiling headers expose event and stream types, and the CUDA smoke tests use
// kernel launch syntax. Pull in the full runtime header so both cases share one compat layer.
#include <cuda_runtime.h>
#else
struct dim3 {
    unsigned int x;
    unsigned int y;
    unsigned int z;

    constexpr dim3(unsigned int vx = 1, unsigned int vy = 1, unsigned int vz = 1) noexcept
        : x(vx), y(vy), z(vz) {
    }
};

using cudaError_t = int;
using cudaEvent_t = void*;
using cudaStream_t = void*;

inline constexpr cudaError_t cudaSuccess = 0;
inline constexpr cudaError_t cudaErrorInvalidValue = 1;
inline constexpr cudaError_t cudaErrorMemoryAllocation = 2;
inline constexpr cudaError_t cudaErrorInvalidDevice = 10;
inline constexpr cudaError_t cudaErrorNoDevice = 100;
#endif

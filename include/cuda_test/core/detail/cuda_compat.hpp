#pragma once

#include <cstddef>

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
#include <cuda_runtime_api.h>
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

inline constexpr cudaError_t cudaSuccess = 0;
inline constexpr cudaError_t cudaErrorInvalidValue = 1;
inline constexpr cudaError_t cudaErrorMemoryAllocation = 2;
inline constexpr cudaError_t cudaErrorInvalidDevice = 10;
inline constexpr cudaError_t cudaErrorNoDevice = 100;
#endif

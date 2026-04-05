#pragma once

#include "cuda_test/analysis/detail/runtime_api.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace cuda_test::analysis {

struct OccupancyInfo {
    int active_blocks_per_sm = 0;
    int active_warps_per_sm = 0;
    int max_warps_per_sm = 0;
    double occupancy_ratio = 0.0;
};

inline OccupancyInfo estimate_occupancy(const void* kernel_func,
                                        int block_size,
                                        std::size_t dynamic_smem = 0,
                                        int device_id = 0) {
    if (kernel_func == nullptr) {
        throw std::invalid_argument("estimate_occupancy requires a non-null kernel function");
    }
    if (block_size <= 0) {
        throw std::invalid_argument("estimate_occupancy requires block_size > 0");
    }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    detail::ScopedDevice scoped_device(device_id);

    int active_blocks = 0;
    CUDA_CHECK(
        cudaOccupancyMaxActiveBlocksPerMultiprocessor(&active_blocks, kernel_func, block_size, dynamic_smem));

    const int warp_size = std::max(1, detail::query_device_attribute(device_id, cudaDevAttrWarpSize));
    const int max_threads_per_sm =
        std::max(0, detail::query_device_attribute(device_id, cudaDevAttrMaxThreadsPerMultiProcessor));
    const int active_warps = active_blocks * ((block_size + warp_size - 1) / warp_size);
    const int max_warps = max_threads_per_sm > 0 ? max_threads_per_sm / warp_size : 0;

    OccupancyInfo info;
    info.active_blocks_per_sm = active_blocks;
    info.active_warps_per_sm = active_warps;
    info.max_warps_per_sm = max_warps;
    info.occupancy_ratio = max_warps > 0 ? std::min(1.0,
                                                    static_cast<double>(active_warps) /
                                                        static_cast<double>(max_warps))
                                         : 0.0;
    return info;
#else
    (void)device_id;
    (void)dynamic_smem;
    detail::throw_cuda_unavailable("estimate_occupancy");
#endif
}

template <typename KernelFunc>
inline int suggest_block_size(KernelFunc kernel_func,
                              std::size_t dynamic_smem = 0,
                              int block_size_limit = 0,
                              int device_id = 0) {
    if constexpr (std::is_pointer_v<std::decay_t<KernelFunc>>) {
        if (kernel_func == nullptr) {
            throw std::invalid_argument("suggest_block_size requires a non-null kernel function");
        }
    }

    if (block_size_limit < 0) {
        throw std::invalid_argument("suggest_block_size requires block_size_limit >= 0");
    }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    detail::ScopedDevice scoped_device(device_id);

    int min_grid_size = 0;
    int block_size = 0;
    CUDA_CHECK(cudaOccupancyMaxPotentialBlockSize(
        &min_grid_size, &block_size, kernel_func, dynamic_smem, block_size_limit));

    if (block_size <= 0) {
        throw std::runtime_error("cudaOccupancyMaxPotentialBlockSize returned a non-positive block size");
    }

    return block_size;
#else
    (void)device_id;
    (void)dynamic_smem;
    (void)block_size_limit;
    detail::throw_cuda_unavailable("suggest_block_size");
#endif
}

} // namespace cuda_test::analysis

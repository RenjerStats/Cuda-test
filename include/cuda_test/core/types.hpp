#pragma once

#include "cuda_test/core/detail/cuda_compat.hpp"

#include <cstddef>

namespace cuda_test::core {

struct KernelLaunchConfig {
    dim3 grid{1, 1, 1};
    dim3 block{1, 1, 1};
    std::size_t shared_mem = 0;
    int device_id = 0;
};

[[nodiscard]] inline bool operator==(const KernelLaunchConfig& lhs,
                                     const KernelLaunchConfig& rhs) noexcept {
    return lhs.grid.x == rhs.grid.x && lhs.grid.y == rhs.grid.y && lhs.grid.z == rhs.grid.z &&
           lhs.block.x == rhs.block.x && lhs.block.y == rhs.block.y && lhs.block.z == rhs.block.z &&
           lhs.shared_mem == rhs.shared_mem && lhs.device_id == rhs.device_id;
}

struct ProfilingBreakdown {
    double h2d_ms = 0.0;
    double kernel_ms = 0.0;
    double d2h_ms = 0.0;
    double total_ms = 0.0;
};

struct RunStats {
    double mean_ms = 0.0;
    double median_ms = 0.0;
    double p95_ms = 0.0;
    double ci95_low = 0.0;
    double ci95_high = 0.0;
    double cv = 0.0;
};

} // namespace cuda_test::core

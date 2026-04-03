#pragma once

#include "cuda_test/core/types.hpp"

#include <stdexcept>

namespace cuda_test::analysis {

inline double transfer_compute_ratio(const core::ProfilingBreakdown& breakdown) {
    if (breakdown.kernel_ms <= 0.0) {
        throw std::invalid_argument("transfer_compute_ratio requires kernel_ms > 0");
    }

    return (breakdown.h2d_ms + breakdown.d2h_ms) / breakdown.kernel_ms;
}

} // namespace cuda_test::analysis

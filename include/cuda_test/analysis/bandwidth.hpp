#pragma once

#include "cuda_test/analysis/detail/runtime_api.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace cuda_test::analysis {

struct BandwidthInfo {
    double theoretical_gbps = 0.0;
    double achieved_gbps = 0.0;
    double utilization_ratio = 0.0;
};

inline BandwidthInfo estimate_bandwidth(std::size_t bytes_transferred,
                                        double kernel_ms,
                                        int device_id = 0) {
    if (kernel_ms <= 0.0) {
        throw std::invalid_argument("estimate_bandwidth requires kernel_ms > 0");
    }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    const int memory_clock_khz =
        detail::query_device_attribute(device_id, cudaDevAttrMemoryClockRate);
    const int memory_bus_width_bits =
        detail::query_device_attribute(device_id, cudaDevAttrGlobalMemoryBusWidth);

    const double memory_clock_hz = static_cast<double>(memory_clock_khz) * 1000.0;
    const double memory_bus_width_bytes = static_cast<double>(memory_bus_width_bits) / 8.0;
    const double theoretical_gbps = (memory_clock_hz * 2.0 * memory_bus_width_bytes) / 1.0e9;
    const double achieved_gbps =
        static_cast<double>(bytes_transferred) / (kernel_ms * 1.0e6);

    BandwidthInfo info;
    info.theoretical_gbps = theoretical_gbps;
    info.achieved_gbps = achieved_gbps;
    info.utilization_ratio = theoretical_gbps > 0.0
                                 ? std::clamp(achieved_gbps / theoretical_gbps, 0.0, 1.0)
                                 : 0.0;
    return info;
#else
    (void)bytes_transferred;
    (void)device_id;
    detail::throw_cuda_unavailable("estimate_bandwidth");
#endif
}

} // namespace cuda_test::analysis

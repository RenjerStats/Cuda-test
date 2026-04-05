#pragma once

#include "cuda_test/analysis/bandwidth.hpp"
#include "cuda_test/analysis/kernel_attributes.hpp"
#include "cuda_test/analysis/metrics.hpp"
#include "cuda_test/analysis/occupancy.hpp"

#include <cstddef>

namespace cuda_test::analysis {

struct KernelFingerprint {
    double transfer_compute_ratio = 0.0;
    double occupancy = 0.0;
    double bandwidth_utilization = 0.0;
    double cv = 0.0;
    double block_sensitivity = 0.0;
    double scaling_exponent = 0.0;
    int num_regs = 0;
    std::size_t local_size_bytes = 0;
    std::size_t shared_size_bytes = 0;
};

inline KernelFingerprint build_fingerprint(const core::ProfilingBreakdown& breakdown,
                                           const core::RunStats& kernel_stats,
                                           const OccupancyInfo& occupancy = {},
                                           const BandwidthInfo& bandwidth = {},
                                           const KernelAttributes& attributes = {},
                                           double block_sensitivity_value = 0.0,
                                           double scaling_exponent_value = 0.0) {
    KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = transfer_compute_ratio(breakdown);
    fingerprint.occupancy = occupancy.occupancy_ratio;
    fingerprint.bandwidth_utilization = bandwidth.utilization_ratio;
    fingerprint.cv = kernel_stats.cv;
    fingerprint.block_sensitivity = block_sensitivity_value;
    fingerprint.scaling_exponent = scaling_exponent_value;
    fingerprint.num_regs = attributes.num_regs;
    fingerprint.local_size_bytes = attributes.local_size_bytes;
    fingerprint.shared_size_bytes = attributes.shared_size_bytes;
    return fingerprint;
}

} // namespace cuda_test::analysis

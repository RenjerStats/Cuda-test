#pragma once

#include "cuda_test/analysis/bandwidth.hpp"
#include "cuda_test/analysis/kernel_attributes.hpp"
#include "cuda_test/analysis/metrics.hpp"
#include "cuda_test/analysis/occupancy.hpp"

#include <cstddef>
#include <optional>

namespace cuda_test::analysis {

struct KernelFingerprint {
    double transfer_compute_ratio = 0.0;
    double occupancy = 0.0;
    bool has_occupancy = false;
    double bandwidth_utilization = 0.0;
    bool has_bandwidth_utilization = false;
    double cv = 0.0;
    double block_sensitivity = 0.0;
    double scaling_exponent = 0.0;
    bool has_scaling_exponent = false;
    int num_regs = 0;
    std::size_t local_size_bytes = 0;
    std::size_t shared_size_bytes = 0;
    bool has_kernel_attributes = false;
};

inline KernelFingerprint build_fingerprint(const core::ProfilingBreakdown& breakdown,
                                           const core::RunStats& kernel_stats,
                                           const std::optional<OccupancyInfo>& occupancy = std::nullopt,
                                           const std::optional<BandwidthInfo>& bandwidth = std::nullopt,
                                           const std::optional<KernelAttributes>& attributes = std::nullopt,
                                           double block_sensitivity_value = 0.0,
                                           const std::optional<double>& scaling_exponent_value = std::nullopt) {
    KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = transfer_compute_ratio(breakdown);
    fingerprint.cv = kernel_stats.cv;
    fingerprint.block_sensitivity = block_sensitivity_value;

    if (occupancy.has_value()) {
        fingerprint.occupancy = occupancy->occupancy_ratio;
        fingerprint.has_occupancy = true;
    }

    if (bandwidth.has_value()) {
        fingerprint.bandwidth_utilization = bandwidth->utilization_ratio;
        fingerprint.has_bandwidth_utilization = true;
    }

    if (scaling_exponent_value.has_value()) {
        fingerprint.scaling_exponent = *scaling_exponent_value;
        fingerprint.has_scaling_exponent = true;
    }

    if (attributes.has_value()) {
        fingerprint.num_regs = attributes->num_regs;
        fingerprint.local_size_bytes = attributes->local_size_bytes;
        fingerprint.shared_size_bytes = attributes->shared_size_bytes;
        fingerprint.has_kernel_attributes = true;
    }

    return fingerprint;
}

} // namespace cuda_test::analysis

#pragma once

#include "cuda_test/analysis/fingerprint.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace cuda_test::analysis {

struct Recommendation {
    std::string tag;
    std::string severity;
    std::string summary;
    std::string suggestion;
};

namespace detail {

inline void add_recommendation(std::vector<Recommendation>& recommendations,
                               std::string_view tag,
                               std::string_view severity,
                               std::string_view summary,
                               std::string_view suggestion) {
    recommendations.push_back(Recommendation{std::string(tag),
                                             std::string(severity),
                                             std::string(summary),
                                             std::string(suggestion)});
}

} // namespace detail

inline std::vector<Recommendation> diagnose(const KernelFingerprint& fingerprint) {
    std::vector<Recommendation> recommendations;

    if (fingerprint.transfer_compute_ratio > 3.0) {
        detail::add_recommendation(
            recommendations,
            "transfer_dominated",
            "critical",
            "Host-device transfers dominate kernel time.",
            "Minimize transfer traffic first: keep intermediates on the device, batch small copies, "
            "use pinned host memory, and overlap copies with compute in non-default streams when it fits.");
    }

    if (fingerprint.occupancy > 0.0 && fingerprint.occupancy < 0.5 && fingerprint.num_regs > 32) {
        detail::add_recommendation(
            recommendations,
            "low_occupancy",
            "warning",
            "Low occupancy plus high register use suggests limited latency hiding.",
            "Inspect register usage with --ptxas-options=-v, try __launch_bounds__ or -maxrregcount "
            "carefully, and retest block sizes in the 128-256 thread range.");
    }

    if (fingerprint.local_size_bytes > 0U) {
        detail::add_recommendation(
            recommendations,
            "local_memory_pressure",
            "warning",
            "Per-thread local memory is non-zero; those accesses are off-chip.",
            "Inspect ptxas lmem output and reduce large per-thread arrays, dynamic indexing, or spilled live "
            "ranges that push data out of registers.");
    }

    if (fingerprint.bandwidth_utilization > 0.8) {
        detail::add_recommendation(
            recommendations,
            "bandwidth_bound",
            "info",
            "The kernel is likely constrained by device-memory bandwidth.",
            "Improve coalescing, remove redundant global-memory traffic, and use shared memory when it can "
            "turn strided traffic into coalesced or cached access. Check for bank conflicts in tiled code.");
    }

    if (fingerprint.cv > 0.15) {
        detail::add_recommendation(
            recommendations,
            "unstable_timing",
            "warning",
            "Timing variation is high enough to weaken benchmark conclusions.",
            "Reduce competing GPU work and contexts, use a quieter benchmark environment or exclusive-process "
            "mode when possible, and only then increase sample count.");
    }

    if (fingerprint.block_sensitivity > 1.3) {
        detail::add_recommendation(
            recommendations,
            "block_sensitive",
            "info",
            "Performance changes materially across block sizes.",
            "Keep block size tunable, benchmark multiples of 32, and start exploration in the 128-256 thread "
            "range after each kernel change.");
    }

    if (fingerprint.scaling_exponent > 1.2) {
        detail::add_recommendation(
            recommendations,
            "superlinear_scaling",
            "warning",
            "Runtime grows faster than near-linear expectations as the problem grows.",
            "Inspect algorithmic complexity, serial sections, synchronization or atomic hotspots, and memory "
            "traffic growth as problem size increases.");
    }

    if (recommendations.empty() && fingerprint.transfer_compute_ratio >= 0.0 &&
        fingerprint.transfer_compute_ratio < 0.3 && fingerprint.occupancy >= 0.6 &&
        fingerprint.cv >= 0.0 && fingerprint.cv < 0.1 && fingerprint.block_sensitivity >= 0.0 &&
        fingerprint.block_sensitivity <= 1.2) {
        detail::add_recommendation(
            recommendations,
            "well_utilized",
            "info",
            "No obvious high-level bottleneck was detected from the available metrics.",
            "Further gains likely require kernel-specific work such as instruction-level parallelism, "
            "warp-level algorithm changes, or deeper profiling with Nsight Compute.");
    }

    return recommendations;
}

} // namespace cuda_test::analysis

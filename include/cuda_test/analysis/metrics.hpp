#pragma once

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/core/types.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cuda_test::analysis {

inline double transfer_compute_ratio(const core::ProfilingBreakdown& breakdown) {
    if (breakdown.kernel_ms <= 0.0) {
        throw std::invalid_argument("transfer_compute_ratio requires kernel_ms > 0");
    }

    return (breakdown.h2d_ms + breakdown.d2h_ms) / breakdown.kernel_ms;
}

inline double block_sensitivity(const std::vector<autotune::CandidateRecord>& candidates) {
    if (candidates.empty()) {
        throw std::invalid_argument("block_sensitivity requires at least one candidate");
    }

    double min_median = std::numeric_limits<double>::infinity();
    double max_median = -std::numeric_limits<double>::infinity();

    for (const autotune::CandidateRecord& candidate : candidates) {
        const double median_ms = candidate.benchmark.kernel_stats.median_ms;
        if (median_ms <= 0.0) {
            throw std::invalid_argument("block_sensitivity requires positive median kernel times");
        }

        min_median = std::min(min_median, median_ms);
        max_median = std::max(max_median, median_ms);
    }

    return max_median / min_median;
}

inline double scaling_exponent(const std::vector<std::pair<std::size_t, double>>& size_time_pairs) {
    if (size_time_pairs.size() < 2U) {
        throw std::invalid_argument("scaling_exponent requires at least two samples");
    }

    double sum_log_sizes = 0.0;
    double sum_log_times = 0.0;

    for (const auto& [size, time_ms] : size_time_pairs) {
        if (size == 0U) {
            throw std::invalid_argument("scaling_exponent requires problem sizes > 0");
        }
        if (time_ms <= 0.0) {
            throw std::invalid_argument("scaling_exponent requires times > 0");
        }

        sum_log_sizes += std::log(static_cast<double>(size));
        sum_log_times += std::log(time_ms);
    }

    const double sample_count = static_cast<double>(size_time_pairs.size());
    const double mean_log_sizes = sum_log_sizes / sample_count;
    const double mean_log_times = sum_log_times / sample_count;

    double numerator = 0.0;
    double denominator = 0.0;
    for (const auto& [size, time_ms] : size_time_pairs) {
        const double centered_log_size = std::log(static_cast<double>(size)) - mean_log_sizes;
        const double centered_log_time = std::log(time_ms) - mean_log_times;
        numerator += centered_log_size * centered_log_time;
        denominator += centered_log_size * centered_log_size;
    }

    if (denominator == 0.0) {
        throw std::invalid_argument("scaling_exponent requires at least two distinct problem sizes");
    }

    return numerator / denominator;
}

} // namespace cuda_test::analysis

#pragma once

#include "cuda_test/core/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace cuda_test::benchmark {

namespace detail {

inline double compute_median_from_sorted(const std::vector<double>& sorted_samples) {
    const std::size_t sample_count = sorted_samples.size();
    const std::size_t middle_index = sample_count / 2;

    if ((sample_count % 2U) != 0U) {
        return sorted_samples[middle_index];
    }

    return (sorted_samples[middle_index - 1] + sorted_samples[middle_index]) / 2.0;
}

inline double compute_percentile_from_sorted(const std::vector<double>& sorted_samples,
                                             double percentile) {
    if (sorted_samples.empty()) {
        throw std::invalid_argument("Percentile requires at least one sample");
    }
    if (percentile < 0.0 || percentile > 1.0) {
        throw std::invalid_argument("Percentile must be in the range [0, 1]");
    }
    if (sorted_samples.size() == 1U) {
        return sorted_samples.front();
    }

    const double rank = percentile * static_cast<double>(sorted_samples.size() - 1U);
    const std::size_t lower_index = static_cast<std::size_t>(std::floor(rank));
    const std::size_t upper_index = static_cast<std::size_t>(std::ceil(rank));

    if (lower_index == upper_index) {
        return sorted_samples[lower_index];
    }

    const double weight = rank - static_cast<double>(lower_index);
    return sorted_samples[lower_index] * (1.0 - weight) + sorted_samples[upper_index] * weight;
}

inline double compute_sample_standard_deviation(const std::vector<double>& samples, double mean) {
    if (samples.size() <= 1U) {
        return 0.0;
    }

    double squared_sum = 0.0;
    for (const double sample : samples) {
        const double delta = sample - mean;
        squared_sum += delta * delta;
    }

    const double variance = squared_sum / static_cast<double>(samples.size() - 1U);
    return std::sqrt(variance);
}

} // namespace detail

inline core::RunStats compute_stats(const std::vector<double>& samples) {
    if (samples.empty()) {
        throw std::invalid_argument("Statistics require at least one sample");
    }

    const double sample_count = static_cast<double>(samples.size());
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = sum / sample_count;

    std::vector<double> sorted_samples = samples;
    std::sort(sorted_samples.begin(), sorted_samples.end());

    const double standard_deviation = detail::compute_sample_standard_deviation(samples, mean);
    const double ci95_margin =
        samples.size() <= 1U ? 0.0 : 1.96 * standard_deviation / std::sqrt(sample_count);

    core::RunStats stats;
    stats.mean_ms = mean;
    stats.median_ms = detail::compute_median_from_sorted(sorted_samples);
    stats.p95_ms = detail::compute_percentile_from_sorted(sorted_samples, 0.95);
    stats.ci95_low = mean - ci95_margin;
    stats.ci95_high = mean + ci95_margin;
    stats.cv = mean == 0.0 ? 0.0 : standard_deviation / mean;
    return stats;
}

} // namespace cuda_test::benchmark

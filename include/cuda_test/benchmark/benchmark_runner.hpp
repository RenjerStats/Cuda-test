#pragma once

#include "cuda_test/benchmark/stats.hpp"
#include "cuda_test/core/types.hpp"

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuda_test::benchmark {

struct BenchmarkConfig {
    int warmup_runs = 5;
    int measure_runs = 30;
};

struct BenchmarkResult {
    std::vector<core::ProfilingBreakdown> samples;
    core::RunStats h2d_stats;
    core::RunStats kernel_stats;
    core::RunStats d2h_stats;
    core::RunStats total_stats;
};

namespace detail {

inline void validate_config(const BenchmarkConfig& config) {
    if (config.warmup_runs < 0) {
        throw std::invalid_argument("Warm-up runs must not be negative");
    }
    if (config.measure_runs <= 0) {
        throw std::invalid_argument("Measured runs must be greater than zero");
    }
}

inline std::vector<double> collect_stage_samples(
    const std::vector<core::ProfilingBreakdown>& breakdowns,
    double core::ProfilingBreakdown::*member) {
    std::vector<double> samples;
    samples.reserve(breakdowns.size());

    for (const core::ProfilingBreakdown& breakdown : breakdowns) {
        samples.push_back(breakdown.*member);
    }

    return samples;
}

inline BenchmarkResult summarize_breakdowns(const std::vector<core::ProfilingBreakdown>& breakdowns) {
    BenchmarkResult result;
    result.samples = breakdowns;
    result.h2d_stats = compute_stats(collect_stage_samples(result.samples, &core::ProfilingBreakdown::h2d_ms));
    result.kernel_stats =
        compute_stats(collect_stage_samples(result.samples, &core::ProfilingBreakdown::kernel_ms));
    result.d2h_stats = compute_stats(collect_stage_samples(result.samples, &core::ProfilingBreakdown::d2h_ms));
    result.total_stats =
        compute_stats(collect_stage_samples(result.samples, &core::ProfilingBreakdown::total_ms));
    return result;
}

} // namespace detail

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(BenchmarkConfig config = {}) : config_(config) {
        detail::validate_config(config_);
    }

    [[nodiscard]] const BenchmarkConfig& config() const noexcept {
        return config_;
    }

    template <typename Callable>
    [[nodiscard]] BenchmarkResult run(Callable&& measured_run) const {
        using Breakdown = std::decay_t<std::invoke_result_t<Callable&>>;
        static_assert(std::is_convertible_v<Breakdown, core::ProfilingBreakdown>,
                      "BenchmarkRunner callable must return core::ProfilingBreakdown");

        for (int iteration = 0; iteration < config_.warmup_runs; ++iteration) {
            (void)measured_run();
        }

        std::vector<core::ProfilingBreakdown> measured_samples;
        measured_samples.reserve(static_cast<std::size_t>(config_.measure_runs));

        for (int iteration = 0; iteration < config_.measure_runs; ++iteration) {
            measured_samples.push_back(core::ProfilingBreakdown(measured_run()));
        }

        return detail::summarize_breakdowns(measured_samples);
    }

private:
    BenchmarkConfig config_{};
};

} // namespace cuda_test::benchmark

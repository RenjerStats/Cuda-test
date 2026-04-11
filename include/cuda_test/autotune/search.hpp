#pragma once

#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/types.hpp"

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuda_test::autotune {

struct AutoTuneSpec {
    std::vector<int> block_sizes;
    std::vector<int> grid_wave_multipliers{1};
    int warmup_runs = 5;
    int measure_runs = 30;
    int max_threads_per_block = 1024;
    int device_id = 0;
    std::size_t shared_mem = 0;
};

struct CandidateRecord {
    core::KernelLaunchConfig config;
    benchmark::BenchmarkResult benchmark;
    int grid_wave_multiplier = 1;
};

struct AutoTuneResult {
    core::KernelLaunchConfig best;
    core::RunStats stats;
    std::vector<CandidateRecord> all_candidates;
    std::string reason;
};

namespace detail {

inline void validate_spec(const AutoTuneSpec& spec) {
    if (spec.block_sizes.empty()) {
        throw std::invalid_argument("Autotune requires at least one block size candidate");
    }
    if (spec.grid_wave_multipliers.empty()) {
        throw std::invalid_argument("Autotune requires at least one grid wave multiplier");
    }
    if (spec.warmup_runs < 0) {
        throw std::invalid_argument("Warm-up runs must not be negative");
    }
    if (spec.measure_runs <= 0) {
        throw std::invalid_argument("Measured runs must be greater than zero");
    }
    if (spec.max_threads_per_block <= 0) {
        throw std::invalid_argument("Maximum threads per block must be positive");
    }

    for (const int block_size : spec.block_sizes) {
        if (block_size <= 0) {
            throw std::invalid_argument("Block size candidates must be positive");
        }
    }

    for (const int wave_multiplier : spec.grid_wave_multipliers) {
        if (wave_multiplier <= 0) {
            throw std::invalid_argument("Grid wave multipliers must be positive");
        }
    }
}

inline int resolve_max_threads_per_block(const AutoTuneSpec& spec) {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    if (core::device_exists(spec.device_id)) {
        return core::get_device_info(spec.device_id).max_threads_per_block;
    }
#endif
    return spec.max_threads_per_block;
}

inline unsigned int compute_grid_x(std::size_t problem_size, int block_size, int wave_multiplier) {
    if (problem_size == 0U) {
        return 0U;
    }

    const std::size_t base_grid =
        (problem_size + static_cast<std::size_t>(block_size) - 1U) / static_cast<std::size_t>(block_size);
    return static_cast<unsigned int>(base_grid * static_cast<std::size_t>(wave_multiplier));
}

inline core::KernelLaunchConfig make_launch_config(const AutoTuneSpec& spec,
                                                   std::size_t problem_size,
                                                   int block_size,
                                                   int wave_multiplier) {
    core::KernelLaunchConfig config;
    config.grid = dim3(compute_grid_x(problem_size, block_size, wave_multiplier), 1, 1);
    config.block = dim3(static_cast<unsigned int>(block_size), 1, 1);
    config.shared_mem = spec.shared_mem;
    config.device_id = spec.device_id;
    return config;
}

inline bool is_valid_candidate(int block_size, int max_threads_per_block) noexcept {
    return block_size <= max_threads_per_block;
}

inline bool is_better_candidate(const CandidateRecord& lhs, const CandidateRecord& rhs) {
    return std::tie(lhs.benchmark.kernel_stats.median_ms,
                    lhs.benchmark.kernel_stats.p95_ms,
                    lhs.benchmark.kernel_stats.cv) <
           std::tie(rhs.benchmark.kernel_stats.median_ms,
                    rhs.benchmark.kernel_stats.p95_ms,
                    rhs.benchmark.kernel_stats.cv);
}

inline std::optional<std::size_t> find_winning_candidate_index(const AutoTuneResult& result) {
    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const CandidateRecord& candidate = result.all_candidates[index];
        if (candidate.config == result.best &&
            candidate.benchmark.kernel_stats.median_ms == result.stats.median_ms &&
            candidate.benchmark.kernel_stats.p95_ms == result.stats.p95_ms &&
            candidate.benchmark.kernel_stats.cv == result.stats.cv) {
            return index;
        }
    }

    return std::nullopt;
}

inline std::size_t find_best_candidate_index(const std::vector<CandidateRecord>& candidates) {
    std::size_t best_index = 0;
    for (std::size_t index = 1; index < candidates.size(); ++index) {
        if (is_better_candidate(candidates[index], candidates[best_index])) {
            best_index = index;
        }
    }
    return best_index;
}

inline std::optional<std::size_t> find_runner_up_index(const std::vector<CandidateRecord>& candidates,
                                                       std::size_t best_index) {
    std::optional<std::size_t> runner_up_index;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (index == best_index) {
            continue;
        }

        if (!runner_up_index.has_value() ||
            is_better_candidate(candidates[index], candidates[*runner_up_index])) {
            runner_up_index = index;
        }
    }
    return runner_up_index;
}

inline std::string build_selection_reason(const CandidateRecord& winner,
                                          const CandidateRecord* runner_up) {
    std::ostringstream stream;
    stream << "Selected launch config: block=" << winner.config.block.x << ", grid=" << winner.config.grid.x
           << ", kernel median_ms=" << winner.benchmark.kernel_stats.median_ms;

    if (runner_up == nullptr) {
        stream << " as the only valid candidate.";
        return stream.str();
    }

    const core::RunStats& winner_stats = winner.benchmark.kernel_stats;
    const core::RunStats& runner_up_stats = runner_up->benchmark.kernel_stats;

    if (winner_stats.median_ms < runner_up_stats.median_ms) {
        stream << " because it has the lowest median_ms.";
    } else if (winner_stats.p95_ms < runner_up_stats.p95_ms) {
        stream << " because it wins the median_ms tie with lower p95_ms.";
    } else if (winner_stats.cv < runner_up_stats.cv) {
        stream << " because it wins the median_ms and p95_ms tie with lower cv.";
    } else {
        stream << " after an exact tie with the closest runner-up.";
    }

    return stream.str();
}

} // namespace detail

template <typename MeasuredRun, typename Validator>
[[nodiscard]] AutoTuneResult tune_kernel(const AutoTuneSpec& spec,
                                         std::size_t problem_size,
                                         MeasuredRun&& measured_run,
                                         Validator&& validator) {
    using Breakdown = std::decay_t<std::invoke_result_t<MeasuredRun&, const core::KernelLaunchConfig&>>;
    using ValidationResult =
        std::decay_t<std::invoke_result_t<Validator&, const core::KernelLaunchConfig&>>;

    static_assert(std::is_convertible_v<Breakdown, core::ProfilingBreakdown>,
                  "Autotune measured_run must return core::ProfilingBreakdown");
    static_assert(std::is_convertible_v<ValidationResult, bool>,
                  "Autotune validator must return a bool-compatible result");

    detail::validate_spec(spec);

    const int max_threads_per_block = detail::resolve_max_threads_per_block(spec);
    const benchmark::BenchmarkRunner runner(
        benchmark::BenchmarkConfig{spec.warmup_runs, spec.measure_runs});

    std::vector<CandidateRecord> accepted_candidates;
    accepted_candidates.reserve(spec.block_sizes.size() * spec.grid_wave_multipliers.size());

    for (const int block_size : spec.block_sizes) {
        if (!detail::is_valid_candidate(block_size, max_threads_per_block)) {
            continue;
        }

        for (const int wave_multiplier : spec.grid_wave_multipliers) {
            const core::KernelLaunchConfig config =
                detail::make_launch_config(spec, problem_size, block_size, wave_multiplier);

            if (!static_cast<bool>(validator(config))) {
                continue;
            }

            CandidateRecord record;
            record.config = config;
            record.grid_wave_multiplier = wave_multiplier;
            record.benchmark = runner.run([&measured_run, &config]() {
                return core::ProfilingBreakdown(measured_run(config));
            });
            accepted_candidates.push_back(std::move(record));
        }
    }

    if (accepted_candidates.empty()) {
        throw std::runtime_error("Autotune did not find any valid candidates");
    }

    const std::size_t best_index = detail::find_best_candidate_index(accepted_candidates);
    const std::optional<std::size_t> runner_up_index =
        detail::find_runner_up_index(accepted_candidates, best_index);

    AutoTuneResult result;
    result.best = accepted_candidates[best_index].config;
    result.stats = accepted_candidates[best_index].benchmark.kernel_stats;
    result.reason = detail::build_selection_reason(
        accepted_candidates[best_index],
        runner_up_index.has_value() ? &accepted_candidates[*runner_up_index] : nullptr);
    result.all_candidates = std::move(accepted_candidates);
    return result;
}

} // namespace cuda_test::autotune

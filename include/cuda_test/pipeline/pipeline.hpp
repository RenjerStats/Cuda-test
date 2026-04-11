#pragma once

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/pipeline/kernel_descriptor.hpp"
#include "cuda_test/pipeline/pipeline_report.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cuda_test::pipeline {

namespace detail {

inline const autotune::CandidateRecord* find_selected_candidate(const autotune::AutoTuneResult& result) {
    const std::optional<std::size_t> winner_index = autotune::detail::find_winning_candidate_index(result);
    if (!winner_index.has_value()) {
        return nullptr;
    }

    return &result.all_candidates[*winner_index];
}

inline core::ProfilingBreakdown make_median_breakdown(const benchmark::BenchmarkResult& result) {
    core::ProfilingBreakdown breakdown;
    breakdown.h2d_ms = result.h2d_stats.median_ms;
    breakdown.kernel_ms = result.kernel_stats.median_ms;
    breakdown.d2h_ms = result.d2h_stats.median_ms;
    breakdown.total_ms = result.total_stats.median_ms;
    return breakdown;
}

inline std::optional<analysis::KernelFingerprint> build_diagnostics_fingerprint(
    const PipelineReport& report) {
    const benchmark::BenchmarkResult* timing_result = nullptr;
    if (report.benchmark_result().has_value()) {
        timing_result = &*report.benchmark_result();
    } else if (report.autotune_result().has_value()) {
        if (const autotune::CandidateRecord* winner = find_selected_candidate(*report.autotune_result());
            winner != nullptr) {
            timing_result = &winner->benchmark;
        }
    }

    if (timing_result == nullptr) {
        return std::nullopt;
    }

    const core::ProfilingBreakdown breakdown = make_median_breakdown(*timing_result);
    if (breakdown.kernel_ms <= 0.0) {
        return std::nullopt;
    }

    double block_sensitivity_value = 0.0;
    if (report.autotune_result().has_value() && !report.autotune_result()->all_candidates.empty()) {
        const bool can_measure_block_sensitivity =
            std::all_of(report.autotune_result()->all_candidates.begin(),
                        report.autotune_result()->all_candidates.end(),
                        [](const autotune::CandidateRecord& candidate) {
                            return candidate.benchmark.kernel_stats.median_ms > 0.0;
                        });

        if (can_measure_block_sensitivity) {
            block_sensitivity_value = analysis::block_sensitivity(report.autotune_result()->all_candidates);
        }
    }

    return analysis::build_fingerprint(
        breakdown, timing_result->kernel_stats, std::nullopt, std::nullopt, std::nullopt, block_sensitivity_value);
}

} // namespace detail

class Pipeline {
public:
    explicit Pipeline(KernelDescriptor descriptor) : descriptor_(std::move(descriptor)) {
    }

    Pipeline& device(int device_id) {
        if (device_id < 0) {
            throw std::invalid_argument("Pipeline device_id must not be negative");
        }

        device_id_ = device_id;
        return *this;
    }

    Pipeline& correctness() {
        correctness_enabled_ = true;
        return *this;
    }

    Pipeline& benchmark(benchmark::BenchmarkConfig config = {}) {
        benchmark_config_ = config;
        return *this;
    }

    Pipeline& autotune(autotune::AutoTuneSpec spec) {
        autotune_spec_ = std::move(spec);
        return *this;
    }

    Pipeline& autotune(std::vector<int> block_sizes, std::vector<int> grid_wave_multipliers = {1}) {
        autotune::AutoTuneSpec spec;
        spec.block_sizes = std::move(block_sizes);
        spec.grid_wave_multipliers = std::move(grid_wave_multipliers);
        return autotune(std::move(spec));
    }

    Pipeline& diagnose() {
        diagnose_enabled_ = true;
        return *this;
    }

    [[nodiscard]] PipelineReport run() const {
        PipelineReport report;
        report.kernel_name_ = descriptor_.name();
        report.device_id_ = device_id_;
        report.correctness_enabled_ = correctness_enabled_;
        report.benchmark_enabled_ = benchmark_config_.has_value();
        report.autotune_enabled_ = autotune_spec_.has_value();
        report.diagnose_enabled_ = diagnose_enabled_;

        std::optional<core::KernelLaunchConfig> baseline_config;
        if (correctness_enabled_ || benchmark_config_.has_value()) {
            baseline_config = descriptor_.baseline_config(device_id_);
        }

        if (correctness_enabled_) {
            report.correctness_passed_ = descriptor_.validate(*baseline_config);
            if (!report.correctness_passed_) {
                return report;
            }
        }

        if (benchmark_config_.has_value()) {
            const benchmark::BenchmarkRunner runner(*benchmark_config_);
            report.benchmark_result_ = runner.run([this, &baseline_config]() {
                return descriptor_.measure(*baseline_config);
            });
        }

        if (autotune_spec_.has_value()) {
            autotune::AutoTuneSpec spec = *autotune_spec_;
            spec.device_id = device_id_;

            report.autotune_result_ = autotune::tune_kernel(
                spec,
                descriptor_.problem_size(),
                [this](const core::KernelLaunchConfig& config) { return descriptor_.measure(config); },
                [this](const core::KernelLaunchConfig& config) { return descriptor_.validate(config); });
        }

        if (diagnose_enabled_) {
            if (const auto fingerprint = detail::build_diagnostics_fingerprint(report);
                fingerprint.has_value()) {
                report.fingerprint_ = *fingerprint;
                report.recommendations_ = analysis::diagnose(*fingerprint);
            }
        }

        return report;
    }

private:
    KernelDescriptor descriptor_;
    int device_id_ = 0;
    bool correctness_enabled_ = false;
    std::optional<benchmark::BenchmarkConfig> benchmark_config_{};
    std::optional<autotune::AutoTuneSpec> autotune_spec_{};
    bool diagnose_enabled_ = false;
};

[[nodiscard]] inline auto make_pipeline(KernelDescriptor descriptor) {
    return Pipeline(std::move(descriptor));
}

} // namespace cuda_test::pipeline

namespace cuda_test {

using pipeline::Pipeline;
using pipeline::PipelineReport;

[[nodiscard]] inline auto make_pipeline(KernelDescriptor descriptor) {
    return pipeline::make_pipeline(std::move(descriptor));
}

} // namespace cuda_test

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

    [[nodiscard]] PipelineReport run() const {
        PipelineReport report;
        report.kernel_name_ = descriptor_.name();
        report.device_id_ = device_id_;
        report.correctness_enabled_ = correctness_enabled_;
        report.benchmark_enabled_ = benchmark_config_.has_value();
        report.autotune_enabled_ = autotune_spec_.has_value();

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

        return report;
    }

private:
    KernelDescriptor descriptor_;
    int device_id_ = 0;
    bool correctness_enabled_ = false;
    std::optional<benchmark::BenchmarkConfig> benchmark_config_{};
    std::optional<autotune::AutoTuneSpec> autotune_spec_{};
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

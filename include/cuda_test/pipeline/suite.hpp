#pragma once

#include "cuda_test/pipeline/pipeline.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cuda_test::pipeline {

class Suite {
public:
    explicit Suite(std::string name) : name_(std::move(name)) {
    }

    Suite& device(int device_id) {
        if (device_id < 0) {
            throw std::invalid_argument("Suite device_id must not be negative");
        }

        device_id_ = device_id;
        return *this;
    }

    Suite& correctness() {
        correctness_enabled_ = true;
        return *this;
    }

    Suite& benchmark(benchmark::BenchmarkConfig config = {}) {
        benchmark_config_ = config;
        return *this;
    }

    Suite& autotune(autotune::AutoTuneSpec spec) {
        autotune_spec_ = std::move(spec);
        return *this;
    }

    Suite& autotune(std::vector<int> block_sizes, std::vector<int> grid_wave_multipliers = {1}) {
        autotune::AutoTuneSpec spec;
        spec.block_sizes = std::move(block_sizes);
        spec.grid_wave_multipliers = std::move(grid_wave_multipliers);
        return autotune(std::move(spec));
    }

    Suite& diagnose() {
        diagnose_enabled_ = true;
        return *this;
    }

    Suite& add(KernelDescriptor descriptor) {
        const std::string& name = descriptor.name();
        const std::string sanitized_name = detail::sanitize_file_component(name);
        const bool already_exists = std::any_of(descriptors_.begin(), descriptors_.end(), [&name](const auto& existing) {
            return existing.name() == name;
        });
        const bool collides_on_export =
            std::any_of(descriptors_.begin(), descriptors_.end(), [&sanitized_name](const auto& existing) {
            return detail::sanitize_file_component(existing.name()) == sanitized_name;
        });
        if (already_exists || collides_on_export) {
            throw std::invalid_argument(
                "Suite requires unique kernel descriptor names and export-safe file stems");
        }

        descriptors_.push_back(std::move(descriptor));
        return *this;
    }

    [[nodiscard]] SuiteReport run_all() const {
        SuiteReport report;
        report.name_ = name_;
        report.reports_.reserve(descriptors_.size());

        for (const KernelDescriptor& descriptor : descriptors_) {
            Pipeline pipeline_run(descriptor);
            pipeline_run.device(device_id_);

            if (correctness_enabled_) {
                pipeline_run.correctness();
            }
            if (benchmark_config_.has_value()) {
                pipeline_run.benchmark(*benchmark_config_);
            }
            if (autotune_spec_.has_value()) {
                pipeline_run.autotune(*autotune_spec_);
            }
            if (diagnose_enabled_) {
                pipeline_run.diagnose();
            }

            report.reports_.push_back(pipeline_run.run());
        }

        return report;
    }

private:
    std::string name_;
    int device_id_ = 0;
    bool correctness_enabled_ = false;
    std::optional<benchmark::BenchmarkConfig> benchmark_config_{};
    std::optional<autotune::AutoTuneSpec> autotune_spec_{};
    bool diagnose_enabled_ = false;
    std::vector<KernelDescriptor> descriptors_;
};

[[nodiscard]] inline auto make_suite(std::string name) {
    return Suite(std::move(name));
}

} // namespace cuda_test::pipeline

namespace cuda_test {

using pipeline::Suite;
using pipeline::SuiteReport;

[[nodiscard]] inline auto make_suite(std::string name) {
    return pipeline::make_suite(std::move(name));
}

} // namespace cuda_test

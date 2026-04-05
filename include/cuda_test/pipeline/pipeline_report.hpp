#pragma once

#include "cuda_test/analysis/advisor.hpp"
#include "cuda_test/analysis/fingerprint.hpp"
#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cuda_test::pipeline {

class PipelineReport;

} // namespace cuda_test::pipeline

namespace cuda_test::reporting {

void export_csv(const std::filesystem::path& path, const pipeline::PipelineReport& report);
void export_json(const std::filesystem::path& path, const pipeline::PipelineReport& report);

} // namespace cuda_test::reporting

namespace cuda_test::pipeline {

namespace detail {

inline std::string sanitize_file_component(const std::string& value) {
    std::string sanitized;
    sanitized.reserve(value.size());

    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0 || character == '_' || character == '-') {
            sanitized.push_back(static_cast<char>(character));
        } else {
            sanitized.push_back('_');
        }
    }

    sanitized.erase(std::unique(sanitized.begin(), sanitized.end(), [](char lhs, char rhs) {
                        return lhs == '_' && rhs == '_';
                    }),
                    sanitized.end());

    if (sanitized.empty()) {
        sanitized = "report";
    }

    return sanitized;
}

inline std::string make_report_stem(const std::string& suite_name, const std::string& kernel_name) {
    const std::string kernel_component = sanitize_file_component(kernel_name);
    if (suite_name.empty()) {
        return kernel_component;
    }

    return sanitize_file_component(suite_name) + "_" + kernel_component;
}

} // namespace detail

class PipelineReport {
public:
    [[nodiscard]] const std::string& kernel_name() const noexcept {
        return kernel_name_;
    }

    [[nodiscard]] int device_id() const noexcept {
        return device_id_;
    }

    [[nodiscard]] bool correctness_enabled() const noexcept {
        return correctness_enabled_;
    }

    [[nodiscard]] bool correctness_passed() const noexcept {
        return correctness_passed_;
    }

    [[nodiscard]] bool benchmark_enabled() const noexcept {
        return benchmark_enabled_;
    }

    [[nodiscard]] bool autotune_enabled() const noexcept {
        return autotune_enabled_;
    }

    [[nodiscard]] bool diagnose_enabled() const noexcept {
        return diagnose_enabled_;
    }

    [[nodiscard]] bool passed() const noexcept {
        return !correctness_enabled_ || correctness_passed_;
    }

    [[nodiscard]] const std::optional<benchmark::BenchmarkResult>& benchmark_result() const noexcept {
        return benchmark_result_;
    }

    [[nodiscard]] const std::optional<autotune::AutoTuneResult>& autotune_result() const noexcept {
        return autotune_result_;
    }

    [[nodiscard]] const std::optional<analysis::KernelFingerprint>& fingerprint() const noexcept {
        return fingerprint_;
    }

    [[nodiscard]] const std::vector<analysis::Recommendation>& recommendations() const noexcept {
        return recommendations_;
    }

    void to_csv(const std::filesystem::path& path) const {
        reporting::export_csv(path, *this);
    }

    void to_json(const std::filesystem::path& path) const {
        reporting::export_json(path, *this);
    }

private:
    std::string kernel_name_;
    int device_id_ = 0;
    bool correctness_enabled_ = false;
    bool correctness_passed_ = false;
    bool benchmark_enabled_ = false;
    bool autotune_enabled_ = false;
    bool diagnose_enabled_ = false;
    std::optional<benchmark::BenchmarkResult> benchmark_result_{};
    std::optional<autotune::AutoTuneResult> autotune_result_{};
    std::optional<analysis::KernelFingerprint> fingerprint_{};
    std::vector<analysis::Recommendation> recommendations_{};

    friend class Pipeline;
    friend class SuiteReport;
};

class SuiteReport {
public:
    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] const std::vector<PipelineReport>& reports() const noexcept {
        return reports_;
    }

    [[nodiscard]] bool all_passed() const noexcept {
        return std::all_of(reports_.begin(), reports_.end(), [](const PipelineReport& report) {
            return report.passed();
        });
    }

    void to_csv(const std::filesystem::path& directory) const {
        std::filesystem::create_directories(directory);

        for (const PipelineReport& report : reports_) {
            const std::string stem = detail::make_report_stem(name_, report.kernel_name());
            report.to_csv(directory / (stem + ".csv"));
        }
    }

    void to_json(const std::filesystem::path& directory) const {
        std::filesystem::create_directories(directory);

        for (const PipelineReport& report : reports_) {
            const std::string stem = detail::make_report_stem(name_, report.kernel_name());
            report.to_json(directory / (stem + ".json"));
        }
    }

private:
    std::string name_;
    std::vector<PipelineReport> reports_;

    friend class Suite;
};

} // namespace cuda_test::pipeline

// Include reporting definitions after PipelineReport is complete to resolve
// the bidirectional dependency between the pipeline and reporting modules.
#include "cuda_test/reporting/export.hpp"

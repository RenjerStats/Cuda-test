#pragma once

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/core/types.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cuda_test::pipeline {

namespace detail {

inline void ensure_parent_directory(const std::filesystem::path& path) {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

inline std::ofstream open_output_file(const std::filesystem::path& path) {
    ensure_parent_directory(path);

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open pipeline report output file");
    }

    stream.imbue(std::locale::classic());
    return stream;
}

inline std::string format_double(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    return stream.str();
}

inline std::string escape_json_string(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8U);

    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }

    return escaped;
}

inline std::string quote_csv(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 2U);
    escaped.push_back('"');

    for (const char character : value) {
        if (character == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(character);
        }
    }

    escaped.push_back('"');
    return escaped;
}

inline std::string format_bool(bool value) {
    return value ? "true" : "false";
}

inline void write_run_stats_json(std::ostream& stream, const core::RunStats& stats) {
    stream << "{\"mean_ms\":" << format_double(stats.mean_ms) << ",\"median_ms\":"
           << format_double(stats.median_ms) << ",\"p95_ms\":" << format_double(stats.p95_ms)
           << ",\"ci95_low\":" << format_double(stats.ci95_low) << ",\"ci95_high\":"
           << format_double(stats.ci95_high) << ",\"cv\":" << format_double(stats.cv) << '}';
}

inline void write_launch_config_json(std::ostream& stream, const core::KernelLaunchConfig& config) {
    stream << "{\"grid\":{\"x\":" << config.grid.x << ",\"y\":" << config.grid.y << ",\"z\":"
           << config.grid.z << "},\"block\":{\"x\":" << config.block.x << ",\"y\":" << config.block.y
           << ",\"z\":" << config.block.z << "},\"shared_mem\":" << config.shared_mem
           << ",\"device_id\":" << config.device_id << '}';
}

inline void write_benchmark_result_json(std::ostream& stream, const benchmark::BenchmarkResult& result) {
    stream << '{';
    stream << "\"sample_count\":" << result.samples.size() << ',';
    stream << "\"h2d_stats\":";
    write_run_stats_json(stream, result.h2d_stats);
    stream << ',';
    stream << "\"kernel_stats\":";
    write_run_stats_json(stream, result.kernel_stats);
    stream << ',';
    stream << "\"d2h_stats\":";
    write_run_stats_json(stream, result.d2h_stats);
    stream << ',';
    stream << "\"total_stats\":";
    write_run_stats_json(stream, result.total_stats);
    stream << '}';
}

inline void write_autotune_result_json(std::ostream& stream, const autotune::AutoTuneResult& result) {
    stream << '{';
    stream << "\"candidate_count\":" << result.all_candidates.size() << ',';
    stream << "\"best\":";
    write_launch_config_json(stream, result.best);
    stream << ',';
    stream << "\"stats\":";
    write_run_stats_json(stream, result.stats);
    stream << ',';
    stream << "\"reason\":\"" << escape_json_string(result.reason) << "\",";
    stream << "\"all_candidates\":[";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        if (index > 0U) {
            stream << ',';
        }

        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        stream << '{';
        stream << "\"grid_wave_multiplier\":" << candidate.grid_wave_multiplier << ',';
        stream << "\"config\":";
        write_launch_config_json(stream, candidate.config);
        stream << ',';
        stream << "\"h2d_stats\":";
        write_run_stats_json(stream, candidate.benchmark.h2d_stats);
        stream << ',';
        stream << "\"kernel_stats\":";
        write_run_stats_json(stream, candidate.benchmark.kernel_stats);
        stream << ',';
        stream << "\"d2h_stats\":";
        write_run_stats_json(stream, candidate.benchmark.d2h_stats);
        stream << ',';
        stream << "\"total_stats\":";
        write_run_stats_json(stream, candidate.benchmark.total_stats);
        stream << '}';
    }

    stream << "]}";
}

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

inline void write_blank_csv_cell(std::ostream& stream, bool add_separator = true) {
    if (add_separator) {
        stream << ',';
    }
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

    [[nodiscard]] bool passed() const noexcept {
        return !correctness_enabled_ || correctness_passed_;
    }

    [[nodiscard]] const std::optional<benchmark::BenchmarkResult>& benchmark_result() const noexcept {
        return benchmark_result_;
    }

    [[nodiscard]] const std::optional<autotune::AutoTuneResult>& autotune_result() const noexcept {
        return autotune_result_;
    }

    void to_csv(const std::filesystem::path& path) const {
        std::ofstream stream = detail::open_output_file(path);

        stream
            << "kernel_name,device_id,correctness_enabled,correctness_passed,passed,benchmark_enabled,"
               "autotune_enabled,benchmark_sample_count,benchmark_kernel_mean_ms,benchmark_kernel_median_ms,"
               "benchmark_kernel_p95_ms,benchmark_kernel_cv,benchmark_total_mean_ms,"
               "autotune_candidate_count,autotune_best_grid_x,autotune_best_block_x,"
               "autotune_best_shared_mem,autotune_best_device_id,autotune_kernel_mean_ms,"
               "autotune_kernel_median_ms,autotune_kernel_p95_ms,autotune_kernel_cv,autotune_reason\n";

        stream << detail::quote_csv(kernel_name_) << ',' << device_id_ << ','
               << detail::format_bool(correctness_enabled_) << ','
               << detail::format_bool(correctness_passed_) << ',' << detail::format_bool(passed()) << ','
               << detail::format_bool(benchmark_enabled_) << ','
               << detail::format_bool(autotune_enabled_);

        if (benchmark_result_.has_value()) {
            stream << ',' << benchmark_result_->samples.size() << ','
                   << detail::format_double(benchmark_result_->kernel_stats.mean_ms) << ','
                   << detail::format_double(benchmark_result_->kernel_stats.median_ms) << ','
                   << detail::format_double(benchmark_result_->kernel_stats.p95_ms) << ','
                   << detail::format_double(benchmark_result_->kernel_stats.cv) << ','
                   << detail::format_double(benchmark_result_->total_stats.mean_ms);
        } else {
            for (int index = 0; index < 6; ++index) {
                detail::write_blank_csv_cell(stream);
            }
        }

        if (autotune_result_.has_value()) {
            stream << ',' << autotune_result_->all_candidates.size() << ',' << autotune_result_->best.grid.x
                   << ',' << autotune_result_->best.block.x << ',' << autotune_result_->best.shared_mem
                   << ',' << autotune_result_->best.device_id << ','
                   << detail::format_double(autotune_result_->stats.mean_ms) << ','
                   << detail::format_double(autotune_result_->stats.median_ms) << ','
                   << detail::format_double(autotune_result_->stats.p95_ms) << ','
                   << detail::format_double(autotune_result_->stats.cv) << ','
                   << detail::quote_csv(autotune_result_->reason);
        } else {
            for (int index = 0; index < 9; ++index) {
                detail::write_blank_csv_cell(stream);
            }
            stream << ',' << detail::quote_csv("");
        }

        stream << '\n';
    }

    void to_json(const std::filesystem::path& path) const {
        std::ofstream stream = detail::open_output_file(path);

        stream << '{';
        stream << "\"kernel_name\":\"" << detail::escape_json_string(kernel_name_) << "\",";
        stream << "\"device_id\":" << device_id_ << ',';
        stream << "\"correctness_enabled\":" << detail::format_bool(correctness_enabled_) << ',';
        stream << "\"correctness_passed\":" << detail::format_bool(correctness_passed_) << ',';
        stream << "\"passed\":" << detail::format_bool(passed()) << ',';
        stream << "\"benchmark_enabled\":" << detail::format_bool(benchmark_enabled_) << ',';
        stream << "\"autotune_enabled\":" << detail::format_bool(autotune_enabled_) << ',';
        stream << "\"benchmark_result\":";
        if (benchmark_result_.has_value()) {
            detail::write_benchmark_result_json(stream, *benchmark_result_);
        } else {
            stream << "null";
        }
        stream << ',';
        stream << "\"autotune_result\":";
        if (autotune_result_.has_value()) {
            detail::write_autotune_result_json(stream, *autotune_result_);
        } else {
            stream << "null";
        }
        stream << '}';
    }

private:
    std::string kernel_name_;
    int device_id_ = 0;
    bool correctness_enabled_ = false;
    bool correctness_passed_ = false;
    bool benchmark_enabled_ = false;
    bool autotune_enabled_ = false;
    std::optional<benchmark::BenchmarkResult> benchmark_result_{};
    std::optional<autotune::AutoTuneResult> autotune_result_{};

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

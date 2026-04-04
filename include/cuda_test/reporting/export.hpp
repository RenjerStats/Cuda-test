#pragma once

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/pipeline/pipeline_report.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cuda_test::reporting {

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
        throw std::runtime_error("Failed to open report output file");
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

inline std::string format_bool(bool value) {
    return value ? "true" : "false";
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

inline bool same_config(const core::KernelLaunchConfig& lhs, const core::KernelLaunchConfig& rhs) {
    return lhs.grid.x == rhs.grid.x && lhs.grid.y == rhs.grid.y && lhs.grid.z == rhs.grid.z &&
           lhs.block.x == rhs.block.x && lhs.block.y == rhs.block.y && lhs.block.z == rhs.block.z &&
           lhs.shared_mem == rhs.shared_mem && lhs.device_id == rhs.device_id;
}

inline std::optional<std::size_t> find_winner_index(const autotune::AutoTuneResult& result) {
    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        if (same_config(candidate.config, result.best) &&
            candidate.benchmark.kernel_stats.median_ms == result.stats.median_ms &&
            candidate.benchmark.kernel_stats.p95_ms == result.stats.p95_ms &&
            candidate.benchmark.kernel_stats.cv == result.stats.cv) {
            return index;
        }
    }

    return std::nullopt;
}

inline void write_run_stats_csv(std::ostream& stream, const core::RunStats& stats) {
    stream << format_double(stats.mean_ms) << ',' << format_double(stats.median_ms) << ','
           << format_double(stats.p95_ms) << ',' << format_double(stats.ci95_low) << ','
           << format_double(stats.ci95_high) << ',' << format_double(stats.cv);
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

inline void write_blank_csv_cell(std::ostream& stream) {
    stream << ',';
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
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        if (index > 0U) {
            stream << ',';
        }

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

} // namespace detail

inline void export_csv(const std::filesystem::path& path, const benchmark::BenchmarkResult& result) {
    std::ofstream stream = detail::open_output_file(path);
    stream << "h2d_mean_ms,h2d_median_ms,h2d_p95_ms,h2d_ci95_low,h2d_ci95_high,h2d_cv,"
              "kernel_mean_ms,kernel_median_ms,kernel_p95_ms,kernel_ci95_low,kernel_ci95_high,kernel_cv,"
              "d2h_mean_ms,d2h_median_ms,d2h_p95_ms,d2h_ci95_low,d2h_ci95_high,d2h_cv,"
              "total_mean_ms,total_median_ms,total_p95_ms,total_ci95_low,total_ci95_high,total_cv\n";
    detail::write_run_stats_csv(stream, result.h2d_stats);
    stream << ',';
    detail::write_run_stats_csv(stream, result.kernel_stats);
    stream << ',';
    detail::write_run_stats_csv(stream, result.d2h_stats);
    stream << ',';
    detail::write_run_stats_csv(stream, result.total_stats);
    stream << '\n';
}

inline void export_json(const std::filesystem::path& path, const benchmark::BenchmarkResult& result) {
    std::ofstream stream = detail::open_output_file(path);
    detail::write_benchmark_result_json(stream, result);
}

inline void export_csv(const std::filesystem::path& path, const autotune::AutoTuneResult& result) {
    std::ofstream stream = detail::open_output_file(path);
    const std::optional<std::size_t> winner_index = detail::find_winner_index(result);

    stream << "is_best,grid_wave_multiplier,grid_x,grid_y,grid_z,block_x,block_y,block_z,shared_mem,device_id,"
              "h2d_mean_ms,h2d_median_ms,h2d_p95_ms,h2d_ci95_low,h2d_ci95_high,h2d_cv,"
              "kernel_mean_ms,kernel_median_ms,kernel_p95_ms,kernel_ci95_low,kernel_ci95_high,kernel_cv,"
              "d2h_mean_ms,d2h_median_ms,d2h_p95_ms,d2h_ci95_low,d2h_ci95_high,d2h_cv,"
              "total_mean_ms,total_median_ms,total_p95_ms,total_ci95_low,total_ci95_high,total_cv\n";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];

        stream << ((winner_index.has_value() && *winner_index == index) ? "true" : "false") << ','
               << candidate.grid_wave_multiplier << ',' << candidate.config.grid.x << ','
               << candidate.config.grid.y << ',' << candidate.config.grid.z << ','
               << candidate.config.block.x << ',' << candidate.config.block.y << ','
               << candidate.config.block.z << ',' << candidate.config.shared_mem << ','
               << candidate.config.device_id << ',';
        detail::write_run_stats_csv(stream, candidate.benchmark.h2d_stats);
        stream << ',';
        detail::write_run_stats_csv(stream, candidate.benchmark.kernel_stats);
        stream << ',';
        detail::write_run_stats_csv(stream, candidate.benchmark.d2h_stats);
        stream << ',';
        detail::write_run_stats_csv(stream, candidate.benchmark.total_stats);
        stream << '\n';
    }
}

inline void export_json(const std::filesystem::path& path, const autotune::AutoTuneResult& result) {
    std::ofstream stream = detail::open_output_file(path);
    const std::optional<std::size_t> winner_index = detail::find_winner_index(result);

    stream << '{';
    stream << "\"candidate_count\":" << result.all_candidates.size() << ',';
    stream << "\"winner_index\":";
    if (winner_index.has_value()) {
        stream << *winner_index;
    } else {
        stream << "null";
    }
    stream << ',';
    stream << "\"best\":";
    detail::write_launch_config_json(stream, result.best);
    stream << ',';
    stream << "\"stats\":";
    detail::write_run_stats_json(stream, result.stats);
    stream << ',';
    stream << "\"reason\":\"" << detail::escape_json_string(result.reason) << "\",";
    stream << "\"all_candidates\":[";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        if (index > 0U) {
            stream << ',';
        }

        stream << '{';
        stream << "\"is_best\":"
               << ((winner_index.has_value() && *winner_index == index) ? "true" : "false") << ',';
        stream << "\"grid_wave_multiplier\":" << candidate.grid_wave_multiplier << ',';
        stream << "\"config\":";
        detail::write_launch_config_json(stream, candidate.config);
        stream << ',';
        stream << "\"h2d_stats\":";
        detail::write_run_stats_json(stream, candidate.benchmark.h2d_stats);
        stream << ',';
        stream << "\"kernel_stats\":";
        detail::write_run_stats_json(stream, candidate.benchmark.kernel_stats);
        stream << ',';
        stream << "\"d2h_stats\":";
        detail::write_run_stats_json(stream, candidate.benchmark.d2h_stats);
        stream << ',';
        stream << "\"total_stats\":";
        detail::write_run_stats_json(stream, candidate.benchmark.total_stats);
        stream << '}';
    }

    stream << "]}";
}

inline void export_csv(const std::filesystem::path& path, const pipeline::PipelineReport& report) {
    std::ofstream stream = detail::open_output_file(path);

    stream << "kernel_name,device_id,correctness_enabled,correctness_passed,passed,benchmark_enabled,"
              "autotune_enabled,benchmark_sample_count,benchmark_kernel_mean_ms,benchmark_kernel_median_ms,"
              "benchmark_kernel_p95_ms,benchmark_kernel_cv,benchmark_total_mean_ms,"
              "autotune_candidate_count,autotune_best_grid_x,autotune_best_block_x,"
              "autotune_best_shared_mem,autotune_best_device_id,autotune_kernel_mean_ms,"
              "autotune_kernel_median_ms,autotune_kernel_p95_ms,autotune_kernel_cv,autotune_reason\n";

    stream << detail::quote_csv(report.kernel_name()) << ',' << report.device_id() << ','
           << detail::format_bool(report.correctness_enabled()) << ','
           << detail::format_bool(report.correctness_passed()) << ','
           << detail::format_bool(report.passed()) << ','
           << detail::format_bool(report.benchmark_enabled()) << ','
           << detail::format_bool(report.autotune_enabled());

    if (report.benchmark_result().has_value()) {
        const benchmark::BenchmarkResult& benchmark_result = *report.benchmark_result();
        stream << ',' << benchmark_result.samples.size() << ','
               << detail::format_double(benchmark_result.kernel_stats.mean_ms) << ','
               << detail::format_double(benchmark_result.kernel_stats.median_ms) << ','
               << detail::format_double(benchmark_result.kernel_stats.p95_ms) << ','
               << detail::format_double(benchmark_result.kernel_stats.cv) << ','
               << detail::format_double(benchmark_result.total_stats.mean_ms);
    } else {
        for (int index = 0; index < 6; ++index) {
            detail::write_blank_csv_cell(stream);
        }
    }

    if (report.autotune_result().has_value()) {
        const autotune::AutoTuneResult& autotune_result = *report.autotune_result();
        stream << ',' << autotune_result.all_candidates.size() << ',' << autotune_result.best.grid.x
               << ',' << autotune_result.best.block.x << ',' << autotune_result.best.shared_mem << ','
               << autotune_result.best.device_id << ','
               << detail::format_double(autotune_result.stats.mean_ms) << ','
               << detail::format_double(autotune_result.stats.median_ms) << ','
               << detail::format_double(autotune_result.stats.p95_ms) << ','
               << detail::format_double(autotune_result.stats.cv) << ','
               << detail::quote_csv(autotune_result.reason);
    } else {
        for (int index = 0; index < 9; ++index) {
            detail::write_blank_csv_cell(stream);
        }
        stream << ',' << detail::quote_csv("");
    }

    stream << '\n';
}

inline void export_json(const std::filesystem::path& path, const pipeline::PipelineReport& report) {
    std::ofstream stream = detail::open_output_file(path);

    stream << '{';
    stream << "\"kernel_name\":\"" << detail::escape_json_string(report.kernel_name()) << "\",";
    stream << "\"device_id\":" << report.device_id() << ',';
    stream << "\"passed\":" << detail::format_bool(report.passed()) << ',';
    stream << "\"benchmark_enabled\":" << detail::format_bool(report.benchmark_enabled()) << ',';
    stream << "\"autotune_enabled\":" << detail::format_bool(report.autotune_enabled()) << ',';
    stream << "\"correctness\":{\"enabled\":" << detail::format_bool(report.correctness_enabled())
           << ",\"passed\":" << detail::format_bool(report.correctness_passed()) << '}';

    if (report.benchmark_result().has_value()) {
        stream << ",\"benchmark\":";
        detail::write_benchmark_result_json(stream, *report.benchmark_result());
    }

    if (report.autotune_result().has_value()) {
        stream << ",\"autotune\":";
        detail::write_autotune_result_json(stream, *report.autotune_result());
    }

    stream << '}';
}

} // namespace cuda_test::reporting

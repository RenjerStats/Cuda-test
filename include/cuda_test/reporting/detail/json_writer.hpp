#pragma once

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/pipeline/pipeline_report.hpp"

#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace cuda_test::reporting::detail {

inline std::string format_double(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    return stream.str();
}

inline std::string format_bool(bool value) {
    return value ? "true" : "false";
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

inline void write_optional_double_json(std::ostream& stream, double value, bool available) {
    if (available) {
        stream << format_double(value);
    } else {
        stream << "null";
    }
}

inline void write_optional_int_json(std::ostream& stream, int value, bool available) {
    if (available) {
        stream << value;
    } else {
        stream << "null";
    }
}

inline void write_optional_size_json(std::ostream& stream, std::size_t value, bool available) {
    if (available) {
        stream << value;
    } else {
        stream << "null";
    }
}

inline void write_fingerprint_json(std::ostream& stream, const analysis::KernelFingerprint& fingerprint) {
    stream << "{\"transfer_compute_ratio\":" << format_double(fingerprint.transfer_compute_ratio)
           << ",\"occupancy\":";
    write_optional_double_json(stream, fingerprint.occupancy, fingerprint.has_occupancy);
    stream << ",\"bandwidth_utilization\":";
    write_optional_double_json(
        stream, fingerprint.bandwidth_utilization, fingerprint.has_bandwidth_utilization);
    stream << ",\"cv\":" << format_double(fingerprint.cv)
           << ",\"block_sensitivity\":" << format_double(fingerprint.block_sensitivity)
           << ",\"scaling_exponent\":";
    write_optional_double_json(stream, fingerprint.scaling_exponent, fingerprint.has_scaling_exponent);
    stream << ",\"num_regs\":";
    write_optional_int_json(stream, fingerprint.num_regs, fingerprint.has_kernel_attributes);
    stream << ",\"local_size_bytes\":";
    write_optional_size_json(stream, fingerprint.local_size_bytes, fingerprint.has_kernel_attributes);
    stream << ",\"shared_size_bytes\":";
    write_optional_size_json(stream, fingerprint.shared_size_bytes, fingerprint.has_kernel_attributes);
    stream << '}';
}

inline void write_recommendation_json(std::ostream& stream, const analysis::Recommendation& recommendation) {
    stream << "{\"tag\":\"" << escape_json_string(recommendation.tag) << "\",\"severity\":\""
           << escape_json_string(recommendation.severity) << "\",\"summary\":\""
           << escape_json_string(recommendation.summary) << "\",\"suggestion\":\""
           << escape_json_string(recommendation.suggestion) << "\"}";
}

inline void write_recommendations_json(std::ostream& stream,
                                       const std::vector<analysis::Recommendation>& recommendations) {
    stream << '[';

    for (std::size_t index = 0; index < recommendations.size(); ++index) {
        if (index > 0U) {
            stream << ',';
        }

        write_recommendation_json(stream, recommendations[index]);
    }

    stream << ']';
}

inline void write_benchmark_result_json(std::ostream& stream, const benchmark::BenchmarkResult& result) {
    stream << '{';
    stream << "\"sample_count\":" << result.samples.size() << ',';
    stream << "\"h2d_stats\":";
    write_run_stats_json(stream, result.h2d_stats);
    stream << ",\"kernel_stats\":";
    write_run_stats_json(stream, result.kernel_stats);
    stream << ",\"d2h_stats\":";
    write_run_stats_json(stream, result.d2h_stats);
    stream << ",\"total_stats\":";
    write_run_stats_json(stream, result.total_stats);
    stream << '}';
}

inline void write_autotune_result_json(std::ostream& stream, const autotune::AutoTuneResult& result) {
    stream << '{';
    stream << "\"candidate_count\":" << result.all_candidates.size() << ",\"best\":";
    write_launch_config_json(stream, result.best);
    stream << ",\"stats\":";
    write_run_stats_json(stream, result.stats);
    stream << ",\"reason\":\"" << escape_json_string(result.reason) << "\",\"all_candidates\":[";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        if (index > 0U) {
            stream << ',';
        }

        stream << '{';
        stream << "\"grid_wave_multiplier\":" << candidate.grid_wave_multiplier << ",\"config\":";
        write_launch_config_json(stream, candidate.config);
        stream << ",\"h2d_stats\":";
        write_run_stats_json(stream, candidate.benchmark.h2d_stats);
        stream << ",\"kernel_stats\":";
        write_run_stats_json(stream, candidate.benchmark.kernel_stats);
        stream << ",\"d2h_stats\":";
        write_run_stats_json(stream, candidate.benchmark.d2h_stats);
        stream << ",\"total_stats\":";
        write_run_stats_json(stream, candidate.benchmark.total_stats);
        stream << '}';
    }

    stream << "]}";
}

inline void write_pipeline_report_json(std::ostream& stream, const pipeline::PipelineReport& report) {
    stream << '{';
    stream << "\"kernel_name\":\"" << escape_json_string(report.kernel_name()) << "\",";
    stream << "\"device_id\":" << report.device_id() << ',';
    stream << "\"passed\":" << format_bool(report.passed()) << ',';
    stream << "\"benchmark_enabled\":" << format_bool(report.benchmark_enabled()) << ',';
    stream << "\"autotune_enabled\":" << format_bool(report.autotune_enabled()) << ',';
    stream << "\"diagnose_enabled\":" << format_bool(report.diagnose_enabled()) << ',';
    stream << "\"correctness\":{\"enabled\":" << format_bool(report.correctness_enabled())
           << ",\"passed\":" << format_bool(report.correctness_passed()) << '}';

    if (report.benchmark_result().has_value()) {
        stream << ",\"benchmark\":";
        write_benchmark_result_json(stream, *report.benchmark_result());
    }

    if (report.autotune_result().has_value()) {
        stream << ",\"autotune\":";
        write_autotune_result_json(stream, *report.autotune_result());
    }

    if (report.fingerprint().has_value()) {
        stream << ",\"fingerprint\":";
        write_fingerprint_json(stream, *report.fingerprint());
    }

    if (report.diagnose_enabled()) {
        stream << ",\"recommendations\":";
        write_recommendations_json(stream, report.recommendations());
    }

    stream << '}';
}

inline void write_suite_report_json(std::ostream& stream, const pipeline::SuiteReport& report) {
    stream << '{';
    stream << "\"name\":\"" << escape_json_string(report.name()) << "\",";
    stream << "\"all_passed\":" << format_bool(report.all_passed()) << ',';
    stream << "\"report_count\":" << report.reports().size() << ',';
    stream << "\"reports\":[";

    for (std::size_t index = 0; index < report.reports().size(); ++index) {
        if (index > 0U) {
            stream << ',';
        }

        write_pipeline_report_json(stream, report.reports()[index]);
    }

    stream << "]}";
}

} // namespace cuda_test::reporting::detail

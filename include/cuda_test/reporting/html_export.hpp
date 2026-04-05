#pragma once

#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/version.hpp"
#include "cuda_test/pipeline/pipeline_report.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cuda_test::reporting {

namespace detail {

struct TimingSelection {
    const benchmark::BenchmarkResult* result = nullptr;
    std::string label;
};

inline void html_ensure_parent_directory(const std::filesystem::path& path) {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

inline std::ofstream html_open_output_file(const std::filesystem::path& path) {
    html_ensure_parent_directory(path);

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open report output file");
    }

    stream.imbue(std::locale::classic());
    return stream;
}

inline std::string html_format_double(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    return stream.str();
}

inline std::string html_format_bool(bool value) {
    return value ? "true" : "false";
}

inline std::string html_escape_json_string(const std::string& value) {
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

inline void html_write_run_stats_json(std::ostream& stream, const core::RunStats& stats) {
    stream << "{\"mean_ms\":" << html_format_double(stats.mean_ms) << ",\"median_ms\":"
           << html_format_double(stats.median_ms) << ",\"p95_ms\":" << html_format_double(stats.p95_ms)
           << ",\"ci95_low\":" << html_format_double(stats.ci95_low) << ",\"ci95_high\":"
           << html_format_double(stats.ci95_high) << ",\"cv\":" << html_format_double(stats.cv) << '}';
}

inline void html_write_launch_config_json(std::ostream& stream, const core::KernelLaunchConfig& config) {
    stream << "{\"grid\":{\"x\":" << config.grid.x << ",\"y\":" << config.grid.y << ",\"z\":"
           << config.grid.z << "},\"block\":{\"x\":" << config.block.x << ",\"y\":" << config.block.y
           << ",\"z\":" << config.block.z << "},\"shared_mem\":" << config.shared_mem
           << ",\"device_id\":" << config.device_id << '}';
}

inline void html_write_fingerprint_json(std::ostream& stream, const analysis::KernelFingerprint& fingerprint) {
    stream << "{\"transfer_compute_ratio\":" << html_format_double(fingerprint.transfer_compute_ratio)
           << ",\"occupancy\":" << html_format_double(fingerprint.occupancy)
           << ",\"bandwidth_utilization\":" << html_format_double(fingerprint.bandwidth_utilization)
           << ",\"cv\":" << html_format_double(fingerprint.cv)
           << ",\"block_sensitivity\":" << html_format_double(fingerprint.block_sensitivity)
           << ",\"scaling_exponent\":" << html_format_double(fingerprint.scaling_exponent)
           << ",\"num_regs\":" << fingerprint.num_regs
           << ",\"local_size_bytes\":" << fingerprint.local_size_bytes
           << ",\"shared_size_bytes\":" << fingerprint.shared_size_bytes << '}';
}

inline void html_write_recommendation_json(std::ostream& stream,
                                           const analysis::Recommendation& recommendation) {
    stream << "{\"tag\":\"" << html_escape_json_string(recommendation.tag) << "\",\"severity\":\""
           << html_escape_json_string(recommendation.severity) << "\",\"summary\":\""
           << html_escape_json_string(recommendation.summary) << "\",\"suggestion\":\""
           << html_escape_json_string(recommendation.suggestion) << "\"}";
}

inline void html_write_recommendations_json(std::ostream& stream,
                                            const std::vector<analysis::Recommendation>& recommendations) {
    stream << '[';
    for (std::size_t index = 0; index < recommendations.size(); ++index) {
        if (index > 0U) {
            stream << ',';
        }

        html_write_recommendation_json(stream, recommendations[index]);
    }
    stream << ']';
}

inline void html_write_benchmark_result_json(std::ostream& stream, const benchmark::BenchmarkResult& result) {
    stream << '{';
    stream << "\"sample_count\":" << result.samples.size() << ',';
    stream << "\"h2d_stats\":";
    html_write_run_stats_json(stream, result.h2d_stats);
    stream << ",\"kernel_stats\":";
    html_write_run_stats_json(stream, result.kernel_stats);
    stream << ",\"d2h_stats\":";
    html_write_run_stats_json(stream, result.d2h_stats);
    stream << ",\"total_stats\":";
    html_write_run_stats_json(stream, result.total_stats);
    stream << '}';
}

inline void html_write_autotune_result_json(std::ostream& stream, const autotune::AutoTuneResult& result) {
    stream << '{';
    stream << "\"candidate_count\":" << result.all_candidates.size() << ",\"best\":";
    html_write_launch_config_json(stream, result.best);
    stream << ",\"stats\":";
    html_write_run_stats_json(stream, result.stats);
    stream << ",\"reason\":\"" << html_escape_json_string(result.reason) << "\",\"all_candidates\":[";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        if (index > 0U) {
            stream << ',';
        }

        stream << '{';
        stream << "\"grid_wave_multiplier\":" << candidate.grid_wave_multiplier << ",\"config\":";
        html_write_launch_config_json(stream, candidate.config);
        stream << ",\"h2d_stats\":";
        html_write_run_stats_json(stream, candidate.benchmark.h2d_stats);
        stream << ",\"kernel_stats\":";
        html_write_run_stats_json(stream, candidate.benchmark.kernel_stats);
        stream << ",\"d2h_stats\":";
        html_write_run_stats_json(stream, candidate.benchmark.d2h_stats);
        stream << ",\"total_stats\":";
        html_write_run_stats_json(stream, candidate.benchmark.total_stats);
        stream << '}';
    }

    stream << "]}";
}

inline void html_write_pipeline_report_json(std::ostream& stream, const pipeline::PipelineReport& report) {
    stream << '{';
    stream << "\"kernel_name\":\"" << html_escape_json_string(report.kernel_name()) << "\",";
    stream << "\"device_id\":" << report.device_id() << ',';
    stream << "\"passed\":" << html_format_bool(report.passed()) << ',';
    stream << "\"benchmark_enabled\":" << html_format_bool(report.benchmark_enabled()) << ',';
    stream << "\"autotune_enabled\":" << html_format_bool(report.autotune_enabled()) << ',';
    stream << "\"diagnose_enabled\":" << html_format_bool(report.diagnose_enabled()) << ',';
    stream << "\"correctness\":{\"enabled\":" << html_format_bool(report.correctness_enabled())
           << ",\"passed\":" << html_format_bool(report.correctness_passed()) << '}';

    if (report.benchmark_result().has_value()) {
        stream << ",\"benchmark\":";
        html_write_benchmark_result_json(stream, *report.benchmark_result());
    }

    if (report.autotune_result().has_value()) {
        stream << ",\"autotune\":";
        html_write_autotune_result_json(stream, *report.autotune_result());
    }

    if (report.fingerprint().has_value()) {
        stream << ",\"fingerprint\":";
        html_write_fingerprint_json(stream, *report.fingerprint());
    }

    if (report.diagnose_enabled()) {
        stream << ",\"recommendations\":";
        html_write_recommendations_json(stream, report.recommendations());
    }

    stream << '}';
}

inline void html_write_suite_report_json(std::ostream& stream, const pipeline::SuiteReport& report) {
    stream << '{';
    stream << "\"name\":\"" << html_escape_json_string(report.name()) << "\",";
    stream << "\"all_passed\":" << html_format_bool(report.all_passed()) << ',';
    stream << "\"report_count\":" << report.reports().size() << ',';
    stream << "\"reports\":[";

    for (std::size_t index = 0; index < report.reports().size(); ++index) {
        if (index > 0U) {
            stream << ',';
        }

        html_write_pipeline_report_json(stream, report.reports()[index]);
    }

    stream << "]}";
}

inline std::string html_escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 16U);

    for (const char character : value) {
        switch (character) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }

    return escaped;
}

inline std::string escape_json_for_html_script(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 32U);

    for (const char character : value) {
        switch (character) {
        case '<':
            escaped += "\\u003C";
            break;
        case '>':
            escaped += "\\u003E";
            break;
        case '&':
            escaped += "\\u0026";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }

    return escaped;
}

inline std::string format_ms(double value) {
    return html_format_double(value) + " ms";
}

inline std::string format_ratio(double value) {
    return html_format_double(value) + "x";
}

inline std::string format_percent(double value) {
    return html_format_double(value * 100.0) + "%";
}

inline std::string format_bytes_compact(std::size_t value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());

    const double kib = 1024.0;
    const double mib = kib * 1024.0;
    const double gib = mib * 1024.0;

    if (static_cast<double>(value) >= gib) {
        stream << std::fixed << std::setprecision(2) << (static_cast<double>(value) / gib) << " GiB";
    } else if (static_cast<double>(value) >= mib) {
        stream << std::fixed << std::setprecision(2) << (static_cast<double>(value) / mib) << " MiB";
    } else if (static_cast<double>(value) >= kib) {
        stream << std::fixed << std::setprecision(2) << (static_cast<double>(value) / kib) << " KiB";
    } else {
        stream << value << " B";
    }

    return stream.str();
}

inline std::string generated_timestamp() {
    const std::time_t current_time = std::time(nullptr);
    std::tm local_tm{};

#if defined(_WIN32)
    localtime_s(&local_tm, &current_time);
#else
    localtime_r(&current_time, &local_tm);
#endif

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(buffer);
}

inline std::string version_string() {
    std::ostringstream stream;
    stream << ::cuda_test::detail::version_major << '.' << ::cuda_test::detail::version_minor << '.'
           << ::cuda_test::detail::version_patch;
    return stream.str();
}

inline std::string device_label(int device_id) {
    try {
        if (core::device_exists(device_id)) {
            const core::DeviceInfo info = core::get_device_info(device_id);
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << info.name << " (device " << info.device_id << ", cc " << info.major << '.' << info.minor;
            if (info.total_global_memory > 0U) {
                stream << ", " << format_bytes_compact(info.total_global_memory);
            }
            stream << ')';
            return stream.str();
        }
    } catch (...) {
        // Fall back to a stable textual label when runtime metadata is unavailable.
    }

    std::ostringstream fallback;
    fallback << "Device " << device_id;
    return fallback.str();
}

inline std::string severity_class(std::string_view severity) {
    if (severity == "critical") {
        return "sev-critical";
    }
    if (severity == "warning") {
        return "sev-warning";
    }
    return "sev-info";
}

inline std::string sanitize_anchor(std::string_view value) {
    std::string anchor;
    anchor.reserve(value.size());

    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            anchor.push_back(static_cast<char>(std::tolower(character)));
        } else if (character == '_' || character == '-') {
            anchor.push_back(static_cast<char>(character));
        } else {
            anchor.push_back('-');
        }
    }

    anchor.erase(std::unique(anchor.begin(), anchor.end(), [](char lhs, char rhs) {
                     return lhs == '-' && rhs == '-';
                 }),
                 anchor.end());

    if (anchor.empty()) {
        anchor = "report";
    }

    return anchor;
}

inline std::string launch_dims_label(const dim3& dims) {
    std::ostringstream stream;
    stream << dims.x << 'x' << dims.y << 'x' << dims.z;
    return stream.str();
}

inline std::string launch_config_label(const core::KernelLaunchConfig& config) {
    std::ostringstream stream;
    stream << "block " << launch_dims_label(config.block) << " / grid " << launch_dims_label(config.grid);
    if (config.shared_mem > 0U) {
        stream << " / shared " << config.shared_mem << " B";
    }
    return stream.str();
}

inline const autotune::CandidateRecord* find_winner_candidate(const autotune::AutoTuneResult& result) {
    const std::optional<std::size_t> winner_index = autotune::detail::find_winning_candidate_index(result);
    if (!winner_index.has_value()) {
        return nullptr;
    }

    return &result.all_candidates[*winner_index];
}

inline const autotune::CandidateRecord* find_winner_candidate(const pipeline::PipelineReport& report) {
    if (!report.autotune_result().has_value()) {
        return nullptr;
    }

    return find_winner_candidate(*report.autotune_result());
}

inline TimingSelection select_primary_timing(const pipeline::PipelineReport& report) {
    if (const autotune::CandidateRecord* winner = find_winner_candidate(report); winner != nullptr) {
        return TimingSelection{&winner->benchmark, "Winning autotune candidate"};
    }

    if (report.benchmark_result().has_value()) {
        return TimingSelection{&*report.benchmark_result(), "Baseline benchmark"};
    }

    return {};
}

inline std::string css_styles() {
    return R"CSS(
:root {
  --bg: #f6f1e7;
  --paper: rgba(255, 252, 245, 0.94);
  --paper-strong: rgba(255, 255, 255, 0.98);
  --ink: #1f2722;
  --muted: #5f6d65;
  --border: #d8cfbf;
  --shadow: 0 24px 60px rgba(54, 40, 22, 0.12);
  --accent: #1d7a63;
  --accent-soft: #dbf1e8;
  --blue: #1f6fb3;
  --blue-soft: #e2f0fb;
  --amber: #c8791b;
  --amber-soft: #fff1df;
  --red: #b84338;
  --red-soft: #fde5df;
  --green: #1f7a63;
  --green-soft: #ddf2e9;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  color: var(--ink);
  font-family: "Trebuchet MS", "Segoe UI", sans-serif;
  background:
    radial-gradient(circle at top left, rgba(31, 122, 99, 0.12), transparent 30%),
    radial-gradient(circle at top right, rgba(31, 111, 179, 0.10), transparent 24%),
    linear-gradient(180deg, #fbf7f0 0%, #f2ebdd 100%);
}
main {
  max-width: 1180px;
  margin: 0 auto;
  padding: 36px 20px 56px;
}
h1, h2, h3 {
  font-family: "Palatino Linotype", "Book Antiqua", Georgia, serif;
  letter-spacing: 0.01em;
  margin: 0;
}
p { line-height: 1.6; }
a { color: var(--blue); }
.hero,
.pipeline-article,
.panel,
.summary-table {
  background: var(--paper);
  border: 1px solid rgba(216, 207, 191, 0.9);
  border-radius: 22px;
  box-shadow: var(--shadow);
}
.hero {
  padding: 28px;
  margin-bottom: 22px;
  background:
    linear-gradient(135deg, rgba(255,255,255,0.94), rgba(249,243,233,0.92)),
    linear-gradient(135deg, rgba(31,122,99,0.08), transparent);
}
.article-header {
  margin-bottom: 18px;
  padding-bottom: 16px;
  border-bottom: 1px solid rgba(216, 207, 191, 0.9);
}
.eyebrow {
  margin: 0 0 10px;
  text-transform: uppercase;
  letter-spacing: 0.16em;
  font-size: 0.78rem;
  font-weight: 700;
  color: var(--muted);
}
.hero h1,
.article-header h2 {
  font-size: clamp(1.8rem, 2.4vw, 3rem);
  margin-bottom: 10px;
}
.meta-line {
  color: var(--muted);
  margin: 0;
}
.badge-row {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-top: 18px;
}
.pill {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  border-radius: 999px;
  padding: 8px 14px;
  font-size: 0.92rem;
  font-weight: 700;
}
.pill.pass { background: var(--green-soft); color: var(--green); }
.pill.fail { background: var(--red-soft); color: var(--red); }
.pill.off { background: #ece7de; color: #635f58; }
.card-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(190px, 1fr));
  gap: 14px;
  margin-bottom: 20px;
}
.metric-card {
  background: var(--paper-strong);
  border: 1px solid rgba(216, 207, 191, 0.9);
  border-radius: 18px;
  padding: 18px;
}
.metric-card .label {
  display: block;
  margin-bottom: 8px;
  font-size: 0.82rem;
  text-transform: uppercase;
  letter-spacing: 0.12em;
  color: var(--muted);
}
.metric-card .value {
  display: block;
  font-size: 1.25rem;
  font-weight: 800;
}
.metric-card .detail {
  display: block;
  margin-top: 8px;
  color: var(--muted);
}
.panel,
.pipeline-article,
.summary-table {
  padding: 22px;
  margin-bottom: 18px;
}
.section-heading {
  margin-bottom: 14px;
}
.section-heading h2,
.section-heading h3 {
  margin-bottom: 6px;
}
.muted { color: var(--muted); }
.empty {
  color: var(--muted);
  background: rgba(246, 241, 231, 0.8);
}
.table-wrap {
  overflow-x: auto;
}
table {
  width: 100%;
  border-collapse: collapse;
  min-width: 640px;
}
th, td {
  padding: 11px 12px;
  border-bottom: 1px solid rgba(216, 207, 191, 0.75);
  text-align: left;
  vertical-align: top;
}
thead th {
  font-size: 0.82rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--muted);
}
tbody tr:nth-child(even) {
  background: rgba(248, 243, 235, 0.65);
}
.winner-row {
  background: rgba(221, 242, 233, 0.85) !important;
}
.winner-chip {
  color: var(--green);
  font-weight: 800;
}
.chart {
  width: 100%;
  height: auto;
  display: block;
  margin-top: 8px;
  border-radius: 16px;
  background: linear-gradient(180deg, rgba(255,255,255,0.78), rgba(245,238,228,0.74));
  border: 1px solid rgba(216, 207, 191, 0.75);
}
.legend {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  margin-top: 12px;
}
.legend span {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  color: var(--muted);
}
.swatch {
  width: 12px;
  height: 12px;
  border-radius: 999px;
}
.rec-list {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
  gap: 14px;
}
.rec-card {
  border-radius: 18px;
  padding: 18px;
  border: 1px solid rgba(216, 207, 191, 0.9);
  background: var(--paper-strong);
}
.rec-card h3 {
  font-size: 1.15rem;
  margin-bottom: 8px;
}
.rec-card .tag {
  display: inline-block;
  margin-bottom: 10px;
  padding: 5px 10px;
  border-radius: 999px;
  font-size: 0.8rem;
  font-weight: 800;
  text-transform: uppercase;
  letter-spacing: 0.08em;
}
.sev-critical {
  border-left: 5px solid var(--red);
  background: linear-gradient(180deg, rgba(255,255,255,0.98), rgba(253,229,223,0.74));
}
.sev-critical .tag {
  background: var(--red-soft);
  color: var(--red);
}
.sev-warning {
  border-left: 5px solid var(--amber);
  background: linear-gradient(180deg, rgba(255,255,255,0.98), rgba(255,241,223,0.76));
}
.sev-warning .tag {
  background: var(--amber-soft);
  color: var(--amber);
}
.sev-info {
  border-left: 5px solid var(--blue);
  background: linear-gradient(180deg, rgba(255,255,255,0.98), rgba(226,240,251,0.72));
}
.sev-info .tag {
  background: var(--blue-soft);
  color: var(--blue);
}
.suite-summary a {
  font-weight: 700;
  text-decoration: none;
}
.raw-note {
  margin-top: 12px;
  color: var(--muted);
}
code {
  font-family: Consolas, "Liberation Mono", monospace;
  font-size: 0.92em;
}
@media (max-width: 820px) {
  main {
    padding: 24px 14px 40px;
  }
  .hero,
  .panel,
  .summary-table,
  .pipeline-article {
    padding: 18px;
    border-radius: 18px;
  }
  table {
    min-width: 560px;
  }
}
)CSS";
}

inline void append_document_open(std::ostream& stream, std::string_view title) {
    stream << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">"
           << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           << "<title>" << html_escape(title) << "</title><style>" << css_styles()
           << "</style></head><body><main>";
}

inline void append_document_close(std::ostream& stream) {
    stream << "</main></body></html>";
}

inline void append_section_heading(std::ostream& stream,
                                   std::string_view eyebrow,
                                   std::string_view title,
                                   std::string_view subtitle = {}) {
    stream << "<div class=\"section-heading\"><p class=\"eyebrow\">" << html_escape(eyebrow)
           << "</p><h2>" << html_escape(title) << "</h2>";
    if (!subtitle.empty()) {
        stream << "<p class=\"muted\">" << html_escape(subtitle) << "</p>";
    }
    stream << "</div>";
}

inline void append_empty_panel(std::ostream& stream,
                               std::string_view eyebrow,
                               std::string_view title,
                               std::string_view message,
                               std::string_view section_id) {
    stream << "<section class=\"panel empty\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, eyebrow, title);
    stream << "<p>" << html_escape(message) << "</p></section>";
}

inline void append_metric_card(std::ostream& stream,
                               std::string_view label,
                               std::string_view value,
                               std::string_view detail_text) {
    stream << "<div class=\"metric-card\"><span class=\"label\">" << html_escape(label)
           << "</span><span class=\"value\">" << html_escape(value) << "</span><span class=\"detail\">"
           << html_escape(detail_text) << "</span></div>";
}

inline void append_pipeline_hero(std::ostream& stream,
                                 const pipeline::PipelineReport& report,
                                 std::string_view eyebrow,
                                 std::string_view heading_tag) {
    const std::string correctness_status = report.correctness_enabled()
                                               ? (report.correctness_passed() ? "Correctness passed"
                                                                              : "Correctness failed")
                                               : "Correctness skipped";

    stream << "<header class=\"hero\"><p class=\"eyebrow\">" << html_escape(eyebrow) << "</p><"
           << heading_tag << '>' << html_escape(report.kernel_name()) << "</" << heading_tag
           << "><p class=\"meta-line\">" << html_escape(device_label(report.device_id()))
           << " | Generated " << html_escape(generated_timestamp()) << " | cuda_test v"
           << html_escape(version_string()) << "</p><div class=\"badge-row\">";

    stream << "<span class=\"pill "
           << (report.correctness_enabled() ? (report.correctness_passed() ? "pass" : "fail") : "off")
           << "\">" << html_escape(correctness_status) << "</span>";
    stream << "<span class=\"pill " << (report.benchmark_enabled() ? "pass" : "off") << "\">"
           << (report.benchmark_enabled() ? "Benchmark enabled" : "Benchmark skipped") << "</span>";
    stream << "<span class=\"pill " << (report.autotune_enabled() ? "pass" : "off") << "\">"
           << (report.autotune_enabled() ? "Autotune enabled" : "Autotune skipped") << "</span>";
    stream << "<span class=\"pill " << (report.diagnose_enabled() ? "pass" : "off") << "\">"
           << (report.diagnose_enabled() ? "Diagnostics enabled" : "Diagnostics skipped") << "</span>";
    stream << "</div></header>";
}

inline void append_pipeline_summary_cards(std::ostream& stream, const pipeline::PipelineReport& report) {
    const std::string pipeline_status =
        report.correctness_enabled() ? (report.correctness_passed() ? "Passed" : "Failed") : "No gate";
    const std::string pipeline_detail = report.correctness_enabled()
                                            ? "Correctness stage executed"
                                            : "Pipeline ran without correctness validation";

    const std::string benchmark_value = report.benchmark_result().has_value()
                                            ? format_ms(report.benchmark_result()->kernel_stats.median_ms)
                                            : "Not run";
    const std::string benchmark_detail =
        report.benchmark_result().has_value() ? "Baseline kernel median" : "No baseline benchmark data";

    const std::string autotune_value =
        report.autotune_result().has_value() ? format_ms(report.autotune_result()->stats.median_ms) : "Not run";
    const std::string autotune_detail =
        report.autotune_result().has_value() ? launch_config_label(report.autotune_result()->best)
                                             : "No autotune sweep available";

    std::string speedup_value = "n/a";
    std::string speedup_detail = "Requires both baseline and autotune data";
    if (report.benchmark_result().has_value() && report.autotune_result().has_value() &&
        report.autotune_result()->stats.median_ms > 0.0) {
        speedup_value = format_ratio(report.benchmark_result()->kernel_stats.median_ms /
                                     report.autotune_result()->stats.median_ms);
        speedup_detail = "Baseline median / best autotune median";
    }

    const std::string diagnostics_value =
        report.diagnose_enabled() ? std::to_string(report.recommendations().size()) : "Not run";
    const std::string diagnostics_detail = report.diagnose_enabled()
                                               ? (report.recommendations().empty()
                                                      ? "No recommendations generated"
                                                      : "Recommendations emitted")
                                               : "Diagnostics stage was skipped";

    stream << "<section class=\"card-grid\">";
    append_metric_card(stream, "Pipeline status", pipeline_status, pipeline_detail);
    append_metric_card(stream, "Baseline median", benchmark_value, benchmark_detail);
    append_metric_card(stream, "Autotune best", autotune_value, autotune_detail);
    append_metric_card(stream, "Speedup", speedup_value, speedup_detail);
    append_metric_card(stream, "Diagnostics", diagnostics_value, diagnostics_detail);
    stream << "</section>";
}

inline void append_correctness_section(std::ostream& stream,
                                       const pipeline::PipelineReport& report,
                                       std::string_view section_id) {
    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Validation", "Correctness");

    if (!report.correctness_enabled()) {
        stream << "<p class=\"muted\">Correctness stage was not run for this pipeline.</p></section>";
        return;
    }

    stream << "<div class=\"badge-row\"><span class=\"pill "
           << (report.correctness_passed() ? "pass" : "fail") << "\">"
           << (report.correctness_passed() ? "Passed" : "Failed") << "</span></div>";
    stream << "<p class=\"raw-note\">Pipeline-level success is defined by the correctness gate when it is enabled."
           << "</p></section>";
}

inline void append_candidate_table(std::ostream& stream, const autotune::AutoTuneResult& result) {
    const std::optional<std::size_t> winner_index = autotune::detail::find_winning_candidate_index(result);

    stream << "<div class=\"table-wrap\"><table><thead><tr><th>Winner</th><th>Block</th><th>Grid</th><th>Wave</th>"
              "<th>Shared</th><th>Median</th><th>P95</th><th>CV</th></tr></thead><tbody>";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        const bool is_winner = winner_index.has_value() && *winner_index == index;

        stream << "<tr" << (is_winner ? " class=\"winner-row\"" : "") << "><td>"
               << (is_winner ? "<span class=\"winner-chip\">best</span>" : "&nbsp;") << "</td><td>"
               << html_escape(launch_dims_label(candidate.config.block)) << "</td><td>"
               << html_escape(launch_dims_label(candidate.config.grid)) << "</td><td>"
               << candidate.grid_wave_multiplier << "</td><td>" << candidate.config.shared_mem << "</td><td>"
               << html_escape(format_ms(candidate.benchmark.kernel_stats.median_ms)) << "</td><td>"
               << html_escape(format_ms(candidate.benchmark.kernel_stats.p95_ms)) << "</td><td>"
               << html_escape(html_format_double(candidate.benchmark.kernel_stats.cv)) << "</td></tr>";
    }

    stream << "</tbody></table></div>";
}

inline void append_candidate_chart(std::ostream& stream, const autotune::AutoTuneResult& result) {
    std::vector<const autotune::CandidateRecord*> candidates;
    candidates.reserve(result.all_candidates.size());
    for (const autotune::CandidateRecord& candidate : result.all_candidates) {
        candidates.push_back(&candidate);
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->benchmark.kernel_stats.median_ms < rhs->benchmark.kernel_stats.median_ms;
    });

    if (candidates.size() > 10U) {
        candidates.resize(10U);
    }

    const autotune::CandidateRecord* winner = find_winner_candidate(result);
    double max_median = 0.0;
    for (const autotune::CandidateRecord* candidate : candidates) {
        max_median = std::max(max_median, candidate->benchmark.kernel_stats.median_ms);
    }
    if (max_median <= 0.0) {
        max_median = 1.0;
    }

    const int row_height = 34;
    const int label_width = 250;
    const int value_width = 110;
    const int bar_area = 760 - label_width - value_width - 32;
    const int chart_height = static_cast<int>(candidates.size()) * row_height + 34;

    stream << "<svg class=\"chart\" viewBox=\"0 0 760 " << chart_height
           << "\" role=\"img\" aria-label=\"Autotune candidate median chart\">";

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = *candidates[index];
        const double fraction = candidate.benchmark.kernel_stats.median_ms / max_median;
        const int y = 18 + static_cast<int>(index) * row_height;
        const int width = static_cast<int>(fraction * static_cast<double>(bar_area));
        const std::string fill = (&candidate == winner) ? "#1d7a63" : "#7da895";
        const std::string label =
            "block " + std::to_string(candidate.config.block.x) + " / grid " + std::to_string(candidate.config.grid.x);

        stream << "<text x=\"12\" y=\"" << (y + 14)
               << "\" font-size=\"12\" fill=\"#4f5b54\">" << html_escape(label) << "</text>";
        stream << "<rect x=\"" << label_width << "\" y=\"" << y << "\" width=\"" << width
               << "\" height=\"18\" rx=\"9\" fill=\"" << fill << "\"></rect>";
        stream << "<text x=\"" << (label_width + width + 10) << "\" y=\"" << (y + 13)
               << "\" font-size=\"12\" fill=\"#1f2722\">"
               << html_escape(format_ms(candidate.benchmark.kernel_stats.median_ms)) << "</text>";
    }

    stream << "</svg>";
}

inline void append_autotune_section(std::ostream& stream,
                                    const pipeline::PipelineReport& report,
                                    std::string_view section_id) {
    if (!report.autotune_result().has_value()) {
        append_empty_panel(stream,
                           "Autotune",
                           "Candidate sweep",
                           "No autotune data available for this report.",
                           section_id);
        return;
    }

    const autotune::AutoTuneResult& result = *report.autotune_result();
    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream,
                           "Autotune",
                           "Candidate sweep",
                           "Every valid candidate is listed below; the chart shows the top 10 by median kernel time.");
    append_candidate_table(stream, result);
    append_candidate_chart(stream, result);
    if (result.all_candidates.size() > 10U) {
        stream << "<p class=\"raw-note\">Showing top 10 of " << result.all_candidates.size()
               << " candidates in the SVG chart.</p>";
    }
    stream << "</section>";
}

inline void append_breakdown_section(std::ostream& stream,
                                     const pipeline::PipelineReport& report,
                                     std::string_view section_id) {
    const TimingSelection timing = select_primary_timing(report);
    if (timing.result == nullptr) {
        append_empty_panel(stream,
                           "Timing",
                           "Stage breakdown",
                           "No timing data available for a breakdown chart.",
                           section_id);
        return;
    }

    const double h2d = timing.result->h2d_stats.median_ms;
    const double kernel = timing.result->kernel_stats.median_ms;
    const double d2h = timing.result->d2h_stats.median_ms;
    const double total = h2d + kernel + d2h;
    if (total <= 0.0) {
        append_empty_panel(stream,
                           "Timing",
                           "Stage breakdown",
                           "Timing data exists but stage medians are zero.",
                           section_id);
        return;
    }

    const int bar_x = 20;
    const int bar_y = 44;
    const int bar_width = 720;
    const int bar_height = 28;
    const int h2d_width = static_cast<int>((h2d / total) * static_cast<double>(bar_width));
    const int kernel_width = static_cast<int>((kernel / total) * static_cast<double>(bar_width));
    const int d2h_width = bar_width - h2d_width - kernel_width;

    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Timing", "Stage breakdown", timing.label);
    stream << "<svg class=\"chart\" viewBox=\"0 0 760 120\" role=\"img\" aria-label=\"Stage breakdown chart\">"
           << "<rect x=\"" << bar_x << "\" y=\"" << bar_y << "\" width=\"" << h2d_width
           << "\" height=\"" << bar_height << "\" rx=\"14\" fill=\"#1f6fb3\"></rect>"
           << "<rect x=\"" << (bar_x + h2d_width) << "\" y=\"" << bar_y << "\" width=\"" << kernel_width
           << "\" height=\"" << bar_height << "\" fill=\"#1d7a63\"></rect>"
           << "<rect x=\"" << (bar_x + h2d_width + kernel_width) << "\" y=\"" << bar_y << "\" width=\""
           << d2h_width << "\" height=\"" << bar_height << "\" rx=\"14\" fill=\"#c8791b\"></rect>"
           << "<text x=\"20\" y=\"26\" font-size=\"13\" fill=\"#4f5b54\">Median stage share</text></svg>";
    stream << "<div class=\"legend\"><span><i class=\"swatch\" style=\"background:#1f6fb3\"></i>H2D "
           << html_escape(format_ms(h2d)) << " (" << html_escape(format_percent(h2d / total))
           << ")</span><span><i class=\"swatch\" style=\"background:#1d7a63\"></i>Kernel "
           << html_escape(format_ms(kernel)) << " (" << html_escape(format_percent(kernel / total))
           << ")</span><span><i class=\"swatch\" style=\"background:#c8791b\"></i>D2H "
           << html_escape(format_ms(d2h)) << " (" << html_escape(format_percent(d2h / total))
           << ")</span></div></section>";
}

inline void append_timing_rows(std::ostream& stream, const benchmark::BenchmarkResult& result) {
    const std::pair<std::string_view, const core::RunStats*> rows[] = {
        {"H2D", &result.h2d_stats},
        {"Kernel", &result.kernel_stats},
        {"D2H", &result.d2h_stats},
        {"Total", &result.total_stats},
    };

    for (const auto& [label, stats] : rows) {
        stream << "<tr><td>" << html_escape(label) << "</td><td>" << html_escape(format_ms(stats->mean_ms))
               << "</td><td>" << html_escape(format_ms(stats->median_ms)) << "</td><td>"
               << html_escape(format_ms(stats->p95_ms)) << "</td><td>"
               << html_escape(format_ms(stats->ci95_low)) << "</td><td>"
               << html_escape(format_ms(stats->ci95_high)) << "</td><td>"
               << html_escape(html_format_double(stats->cv)) << "</td></tr>";
    }
}

inline void append_timing_section(std::ostream& stream,
                                  const pipeline::PipelineReport& report,
                                  std::string_view section_id) {
    const TimingSelection timing = select_primary_timing(report);
    if (timing.result == nullptr) {
        append_empty_panel(stream,
                           "Timing",
                           "Timing summary",
                           "No timing data available for this report.",
                           section_id);
        return;
    }

    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Timing", "Timing summary", timing.label);
    stream << "<div class=\"table-wrap\"><table><thead><tr><th>Stage</th><th>Mean</th><th>Median</th><th>P95</th>"
              "<th>CI95 low</th><th>CI95 high</th><th>CV</th></tr></thead><tbody>";
    append_timing_rows(stream, *timing.result);
    stream << "</tbody></table></div></section>";
}

inline void append_fingerprint_summary(std::ostream& stream, const analysis::KernelFingerprint& fingerprint) {
    stream << "<p class=\"raw-note\">Fingerprint: occupancy "
           << html_escape(format_percent(fingerprint.occupancy)) << ", bandwidth "
           << html_escape(format_percent(fingerprint.bandwidth_utilization)) << ", transfer/compute "
           << html_escape(html_format_double(fingerprint.transfer_compute_ratio)) << ", registers "
           << fingerprint.num_regs << ", local memory " << fingerprint.local_size_bytes << " B.</p>";
}

inline void append_recommendations_section(std::ostream& stream,
                                           const pipeline::PipelineReport& report,
                                           std::string_view section_id) {
    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Diagnostics", "Recommendations");

    if (!report.diagnose_enabled()) {
        stream << "<p class=\"muted\">Diagnostics were not run for this pipeline.</p></section>";
        return;
    }

    if (report.fingerprint().has_value()) {
        append_fingerprint_summary(stream, *report.fingerprint());
    }

    if (report.recommendations().empty()) {
        stream << "<p class=\"muted\">No recommendations generated.</p></section>";
        return;
    }

    stream << "<div class=\"rec-list\">";
    for (const analysis::Recommendation& recommendation : report.recommendations()) {
        stream << "<article class=\"rec-card " << html_escape(severity_class(recommendation.severity))
               << "\"><span class=\"tag\">" << html_escape(recommendation.severity) << "</span><h3>"
               << html_escape(recommendation.summary) << "</h3><p><strong>Tag:</strong> <code>"
               << html_escape(recommendation.tag) << "</code></p><p>" << html_escape(recommendation.suggestion)
               << "</p></article>";
    }
    stream << "</div></section>";
}

inline void append_pipeline_content(std::ostream& stream,
                                    const pipeline::PipelineReport& report,
                                    std::string_view prefix,
                                    bool nested) {
    if (nested) {
        stream << "<article class=\"pipeline-article\" id=\"" << html_escape(prefix)
               << "\"><div class=\"article-header\"><p class=\"eyebrow\">Kernel section</p><h2>"
               << html_escape(report.kernel_name()) << "</h2><p class=\"meta-line\">"
               << html_escape(device_label(report.device_id())) << "</p></div>";
    } else {
        append_pipeline_hero(stream, report, "cuda_test pipeline report", "h1");
    }

    append_pipeline_summary_cards(stream, report);
    append_correctness_section(stream, report, std::string(prefix) + "-correctness");
    append_autotune_section(stream, report, std::string(prefix) + "-autotune");
    append_breakdown_section(stream, report, std::string(prefix) + "-breakdown");
    append_timing_section(stream, report, std::string(prefix) + "-timing");
    append_recommendations_section(stream, report, std::string(prefix) + "-recommendations");

    if (nested) {
        stream << "</article>";
    }
}

inline void append_raw_json_note(std::ostream& stream) {
    stream << "<section class=\"panel\"><div class=\"section-heading\"><p class=\"eyebrow\">Raw data</p><h2>"
              "Embedded machine-readable JSON</h2><p class=\"muted\">The full report payload is stored in an "
              "inline <code>application/json</code> script tag for downstream tooling.</p></div></section>";
}

inline void append_suite_hero(std::ostream& stream, const pipeline::SuiteReport& report) {
    stream << "<header class=\"hero\"><p class=\"eyebrow\">cuda_test suite report</p><h1>"
           << html_escape(report.name().empty() ? std::string("Unnamed suite") : report.name())
           << "</h1><p class=\"meta-line\">" << report.reports().size() << " kernel report(s) | Generated "
           << html_escape(generated_timestamp()) << " | cuda_test v" << html_escape(version_string())
           << "</p><div class=\"badge-row\"><span class=\"pill " << (report.all_passed() ? "pass" : "fail")
           << "\">" << (report.all_passed() ? "All reports passed" : "At least one report failed")
           << "</span></div></header>";
}

inline std::optional<double> suite_baseline_median(const pipeline::PipelineReport& report) {
    if (report.benchmark_result().has_value()) {
        return report.benchmark_result()->kernel_stats.median_ms;
    }
    return std::nullopt;
}

inline std::optional<double> suite_best_median(const pipeline::PipelineReport& report) {
    if (report.autotune_result().has_value()) {
        return report.autotune_result()->stats.median_ms;
    }
    if (report.benchmark_result().has_value()) {
        return report.benchmark_result()->kernel_stats.median_ms;
    }
    return std::nullopt;
}

inline void append_suite_summary(std::ostream& stream, const pipeline::SuiteReport& report) {
    stream << "<section class=\"summary-table suite-summary\" id=\"suite-summary\">";
    append_section_heading(stream,
                           "Overview",
                           "Kernel summary",
                           "Best-config timing is taken from autotune when available; otherwise the baseline benchmark is shown.");

    if (report.reports().empty()) {
        stream << "<p class=\"muted\">No kernels tested.</p></section>";
        return;
    }

    stream << "<div class=\"table-wrap\"><table><thead><tr><th>Kernel</th><th>Status</th><th>Best config</th>"
              "<th>Baseline median</th><th>Best median</th><th>Speedup</th><th>Findings</th></tr></thead><tbody>";

    for (const pipeline::PipelineReport& kernel_report : report.reports()) {
        const std::string anchor = "kernel-" + sanitize_anchor(kernel_report.kernel_name());
        const std::optional<double> baseline = suite_baseline_median(kernel_report);
        const std::optional<double> best = suite_best_median(kernel_report);

        std::string best_config = "baseline";
        if (kernel_report.autotune_result().has_value()) {
            best_config = launch_config_label(kernel_report.autotune_result()->best);
        }

        std::string speedup = "n/a";
        if (baseline.has_value() && best.has_value() && *best > 0.0 && kernel_report.autotune_result().has_value()) {
            speedup = format_ratio(*baseline / *best);
        }

        stream << "<tr><td><a href=\"#" << html_escape(anchor) << "\">" << html_escape(kernel_report.kernel_name())
               << "</a></td><td>" << (kernel_report.passed() ? "Passed" : "Failed") << "</td><td>"
               << html_escape(best_config) << "</td><td>"
               << html_escape(baseline.has_value() ? format_ms(*baseline) : std::string("n/a")) << "</td><td>"
               << html_escape(best.has_value() ? format_ms(*best) : std::string("n/a")) << "</td><td>"
               << html_escape(speedup) << "</td><td>" << kernel_report.recommendations().size() << "</td></tr>";
    }

    stream << "</tbody></table></div></section>";
}

inline std::string pipeline_raw_json(const pipeline::PipelineReport& report) {
    std::ostringstream stream;
    html_write_pipeline_report_json(stream, report);
    return stream.str();
}

inline std::string suite_raw_json(const pipeline::SuiteReport& report) {
    std::ostringstream stream;
    html_write_suite_report_json(stream, report);
    return stream.str();
}

inline std::string render_pipeline_html(const pipeline::PipelineReport& report) {
    std::ostringstream stream;
    const std::string title = "cuda_test Report: " + report.kernel_name();
    const std::string prefix = "report-" + sanitize_anchor(report.kernel_name());

    append_document_open(stream, title);
    append_pipeline_content(stream, report, prefix, false);
    append_raw_json_note(stream);
    stream << "<script type=\"application/json\" id=\"raw-data\">"
           << escape_json_for_html_script(pipeline_raw_json(report)) << "</script>";
    append_document_close(stream);
    return stream.str();
}

inline std::string render_suite_html(const pipeline::SuiteReport& report) {
    std::ostringstream stream;
    const std::string title =
        "cuda_test Suite Report: " + (report.name().empty() ? std::string("unnamed") : report.name());

    append_document_open(stream, title);
    append_suite_hero(stream, report);
    append_suite_summary(stream, report);

    for (const pipeline::PipelineReport& kernel_report : report.reports()) {
        append_pipeline_content(stream, kernel_report, "kernel-" + sanitize_anchor(kernel_report.kernel_name()), true);
    }

    append_raw_json_note(stream);
    stream << "<script type=\"application/json\" id=\"suite-raw-data\">"
           << escape_json_for_html_script(suite_raw_json(report)) << "</script>";
    append_document_close(stream);
    return stream.str();
}

} // namespace detail

inline void export_html(const std::filesystem::path& path, const pipeline::PipelineReport& report) {
    std::ofstream stream = detail::html_open_output_file(path);
    stream << detail::render_pipeline_html(report);
}

inline void export_html(const std::filesystem::path& path, const pipeline::SuiteReport& report) {
    std::ofstream stream = detail::html_open_output_file(path);
    stream << detail::render_suite_html(report);
}

} // namespace cuda_test::reporting

#pragma once

#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/version.hpp"
#include "cuda_test/pipeline/pipeline_report.hpp"
#include "cuda_test/reporting/detail/json_writer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
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

struct RelativeEffectSummary {
    std::string value = "н/д";
    std::string detail = "Нужны данные базового замера и автотюнинга";
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

inline std::string html_format_display_double(double value, int precision) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
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

inline std::string format_scalar(double value) {
    return html_format_display_double(value, 5);
}

inline std::string format_ms(double value) {
    return format_scalar(value) + " ms";
}

inline std::string format_ratio(double value) {
    return format_scalar(value) + "x";
}

inline std::string format_percent(double value) {
    return html_format_display_double(value * 100.0, 1) + "%";
}

inline std::string format_optional_scalar(double value, bool available) {
    return available ? format_scalar(value) : "н/д";
}

inline std::string format_optional_percent(double value, bool available) {
    return available ? format_percent(value) : "н/д";
}

inline std::string format_optional_int(int value, bool available) {
    return available ? std::to_string(value) : "н/д";
}

inline std::string format_optional_bytes(std::size_t value, bool available) {
    return available ? (std::to_string(value) + " Б") : "н/д";
}

inline bool confidence_intervals_overlap(const core::RunStats& lhs, const core::RunStats& rhs) noexcept {
    return lhs.ci95_low <= rhs.ci95_high && rhs.ci95_low <= lhs.ci95_high;
}

inline RelativeEffectSummary summarize_relative_effect(const core::RunStats& baseline,
                                                       const core::RunStats& candidate) {
    if (baseline.median_ms <= 0.0 || candidate.median_ms <= 0.0) {
        return {};
    }

    const double ratio = baseline.median_ms / candidate.median_ms;
    const bool intervals_overlap = confidence_intervals_overlap(baseline, candidate);
    const bool medians_are_close = std::fabs(ratio - 1.0) <= 0.15;

    if (intervals_overlap && medians_are_close) {
        return RelativeEffectSummary{
            "в пределах шума",
            "95% доверительные интервалы базового замера и лучшего кандидата автотюнинга пересекаются",
        };
    }
    if (ratio > 1.0) {
        return RelativeEffectSummary{
            "ускорение " + format_ratio(ratio),
            "Лучший кандидат автотюнинга быстрее базового замера",
        };
    }

    if (ratio < 1.0) {
        return RelativeEffectSummary{
            "замедление " + format_ratio(candidate.median_ms / baseline.median_ms),
            "Лучший кандидат автотюнинга медленнее базового замера",
        };
    }

    return RelativeEffectSummary{
        "без изменений",
        "Медианы базового замера и автотюнинга совпадают",
    };
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

inline int breakdown_segment_radius(int width, int height) noexcept {
    if (width <= 0 || height <= 0) {
        return 0;
    }

    return std::min(14, std::min(width / 2, height / 2));
}

inline std::string device_label(int device_id) {
    try {
        if (core::device_exists(device_id)) {
            const core::DeviceInfo info = core::get_device_info(device_id);
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << info.name << " (устройство " << info.device_id << ", cc " << info.major << '.'
                   << info.minor;
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
    fallback << "Устройство " << device_id;
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
    stream << "блок " << launch_dims_label(config.block) << " / сетка " << launch_dims_label(config.grid);
    if (config.shared_mem > 0U) {
        stream << " / разделяемая память " << config.shared_mem << " Б";
    }
    return stream.str();
}

inline std::string severity_label(std::string_view severity) {
    if (severity == "critical") {
        return "критично";
    }
    if (severity == "warning") {
        return "предупреждение";
    }
    return "информация";
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
        return TimingSelection{&winner->benchmark, "Лучший кандидат автотюнинга"};
    }

    if (report.benchmark_result().has_value()) {
        return TimingSelection{&*report.benchmark_result(), "Базовый замер"};
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
    stream << "<!DOCTYPE html><html lang=\"ru\"><head><meta charset=\"UTF-8\">"
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
                                               ? (report.correctness_passed() ? "Проверка пройдена"
                                                                              : "Проверка не пройдена")
                                               : "Проверка пропущена";

    stream << "<header class=\"hero\"><p class=\"eyebrow\">" << html_escape(eyebrow) << "</p><"
           << heading_tag << '>' << html_escape(report.kernel_name()) << "</" << heading_tag
           << "><p class=\"meta-line\">" << html_escape(device_label(report.device_id()))
           << " | Сформировано " << html_escape(generated_timestamp()) << " | cuda_test v"
           << html_escape(version_string()) << "</p><div class=\"badge-row\">";

    stream << "<span class=\"pill "
           << (report.correctness_enabled() ? (report.correctness_passed() ? "pass" : "fail") : "off")
           << "\">" << html_escape(correctness_status) << "</span>";
    stream << "<span class=\"pill " << (report.benchmark_enabled() ? "pass" : "off") << "\">"
           << (report.benchmark_enabled() ? "Бенчмарк включен" : "Бенчмарк пропущен") << "</span>";
    stream << "<span class=\"pill " << (report.autotune_enabled() ? "pass" : "off") << "\">"
           << (report.autotune_enabled() ? "Автотюнинг включен" : "Автотюнинг пропущен") << "</span>";
    stream << "<span class=\"pill " << (report.diagnose_enabled() ? "pass" : "off") << "\">"
           << (report.diagnose_enabled() ? "Диагностика включена" : "Диагностика пропущена") << "</span>";
    stream << "</div></header>";
}

inline void append_pipeline_summary_cards(std::ostream& stream, const pipeline::PipelineReport& report) {
    const std::string pipeline_status =
        report.correctness_enabled() ? (report.correctness_passed() ? "Пройден" : "Не пройден")
                                     : "Без проверки";
    const std::string pipeline_detail = report.correctness_enabled()
                                            ? "Этап проверки корректности выполнен"
                                            : "Пайплайн выполнен без проверки корректности";

    const std::string benchmark_value = report.benchmark_result().has_value()
                                            ? format_ms(report.benchmark_result()->kernel_stats.median_ms)
                                            : "Не запускался";
    const std::string benchmark_detail =
        report.benchmark_result().has_value() ? "Медиана времени ядра в базовой конфигурации"
                                              : "Нет данных базового замера";

    const std::string autotune_value =
        report.autotune_result().has_value() ? format_ms(report.autotune_result()->stats.median_ms)
                                             : "Не запускался";
    const std::string autotune_detail =
        report.autotune_result().has_value() ? launch_config_label(report.autotune_result()->best)
                                             : "Нет результатов автотюнинга";

    RelativeEffectSummary relative_effect;
    if (report.benchmark_result().has_value() && report.autotune_result().has_value() &&
        report.autotune_result()->stats.median_ms > 0.0) {
        relative_effect = summarize_relative_effect(report.benchmark_result()->kernel_stats,
                                                    report.autotune_result()->stats);
    }

    const std::string diagnostics_value =
        report.diagnose_enabled() ? std::to_string(report.recommendations().size()) : "Не запускалась";
    const std::string diagnostics_detail = report.diagnose_enabled()
                                               ? (report.recommendations().empty()
                                                      ? "Рекомендации не сгенерированы"
                                                      : "Рекомендации сгенерированы")
                                               : "Этап диагностики был пропущен";

    stream << "<section class=\"card-grid\">";
    append_metric_card(stream, "Статус пайплайна", pipeline_status, pipeline_detail);
    append_metric_card(stream, "Медиана базового замера", benchmark_value, benchmark_detail);
    append_metric_card(stream, "Лучший автотюнинг", autotune_value, autotune_detail);
    append_metric_card(stream, "Эффект", relative_effect.value, relative_effect.detail);
    append_metric_card(stream, "Диагностика", diagnostics_value, diagnostics_detail);
    stream << "</section>";
}

inline void append_correctness_section(std::ostream& stream,
                                       const pipeline::PipelineReport& report,
                                       std::string_view section_id) {
    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Проверка", "Корректность");

    if (!report.correctness_enabled()) {
        stream << "<p class=\"muted\">Проверка корректности для этого пайплайна не запускалась.</p></section>";
        return;
    }

    stream << "<div class=\"badge-row\"><span class=\"pill "
           << (report.correctness_passed() ? "pass" : "fail") << "\">"
           << (report.correctness_passed() ? "Пройдено" : "Ошибка") << "</span></div>";
    stream << "<p class=\"raw-note\">Успех пайплайна определяется этапом проверки корректности, если он включен."
           << "</p></section>";
}

inline void append_candidate_table(std::ostream& stream, const autotune::AutoTuneResult& result) {
    const std::optional<std::size_t> winner_index = autotune::detail::find_winning_candidate_index(result);

    stream << "<div class=\"table-wrap\"><table><thead><tr><th>Лидер</th><th>Блок</th><th>Сетка</th><th>Волна</th>"
              "<th>Разделяемая память, Б</th><th>Median</th><th>P95</th><th>CV</th></tr></thead><tbody>";

    for (std::size_t index = 0; index < result.all_candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = result.all_candidates[index];
        const bool is_winner = winner_index.has_value() && *winner_index == index;

        stream << "<tr" << (is_winner ? " class=\"winner-row\"" : "") << "><td>"
               << (is_winner ? "<span class=\"winner-chip\">лучший</span>" : "&nbsp;") << "</td><td>"
               << html_escape(launch_dims_label(candidate.config.block)) << "</td><td>"
               << html_escape(launch_dims_label(candidate.config.grid)) << "</td><td>"
               << candidate.grid_wave_multiplier << "</td><td>" << candidate.config.shared_mem << "</td><td>"
               << html_escape(format_ms(candidate.benchmark.kernel_stats.median_ms)) << "</td><td>"
               << html_escape(format_ms(candidate.benchmark.kernel_stats.p95_ms)) << "</td><td>"
               << html_escape(format_scalar(candidate.benchmark.kernel_stats.cv)) << "</td></tr>";
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
           << "\" role=\"img\" aria-label=\"График медиан кандидатов автотюнинга\">";

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const autotune::CandidateRecord& candidate = *candidates[index];
        const double fraction = candidate.benchmark.kernel_stats.median_ms / max_median;
        const int y = 18 + static_cast<int>(index) * row_height;
        const int width = static_cast<int>(fraction * static_cast<double>(bar_area));
        const std::string fill = (&candidate == winner) ? "#1d7a63" : "#7da895";
        const std::string label =
            "блок " + std::to_string(candidate.config.block.x) + " / сетка " +
            std::to_string(candidate.config.grid.x);

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
                           "Автотюнинг",
                           "Перебор кандидатов",
                           "Для этого отчета нет данных автотюнинга.",
                           section_id);
        return;
    }

    const autotune::AutoTuneResult& result = *report.autotune_result();
    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream,
                           "Автотюнинг",
                           "Перебор кандидатов",
                           "Ниже перечислены все валидные кандидаты; на графике показаны 10 лучших по медиане времени ядра.");
    append_candidate_table(stream, result);
    append_candidate_chart(stream, result);
    if (result.all_candidates.size() > 10U) {
        stream << "<p class=\"raw-note\">На SVG-графике показаны 10 лучших кандидатов из "
               << result.all_candidates.size() << " проверенных конфигураций.</p>";
    }
    stream << "</section>";
}

inline void append_breakdown_section(std::ostream& stream,
                                     const pipeline::PipelineReport& report,
                                     std::string_view section_id) {
    const TimingSelection timing = select_primary_timing(report);
    if (timing.result == nullptr) {
        append_empty_panel(stream,
                           "Время",
                           "Разбивка по этапам",
                           "Нет временных данных для построения диаграммы.",
                           section_id);
        return;
    }

    const double h2d = timing.result->h2d_stats.median_ms;
    const double kernel = timing.result->kernel_stats.median_ms;
    const double d2h = timing.result->d2h_stats.median_ms;
    const double total = h2d + kernel + d2h;
    if (total <= 0.0) {
        append_empty_panel(stream,
                           "Время",
                           "Разбивка по этапам",
                           "Временные данные есть, но медианы этапов равны нулю.",
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
    const int h2d_radius = breakdown_segment_radius(h2d_width, bar_height);
    const int d2h_radius = breakdown_segment_radius(d2h_width, bar_height);

    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Время", "Разбивка по этапам", timing.label);
    stream << "<svg class=\"chart\" viewBox=\"0 0 760 120\" role=\"img\" aria-label=\"Диаграмма этапов\">"
           << "<rect x=\"" << bar_x << "\" y=\"" << bar_y << "\" width=\"" << h2d_width
           << "\" height=\"" << bar_height << "\" rx=\"" << h2d_radius << "\" fill=\"#1f6fb3\"></rect>"
           << "<rect x=\"" << (bar_x + h2d_width) << "\" y=\"" << bar_y << "\" width=\"" << kernel_width
           << "\" height=\"" << bar_height << "\" fill=\"#1d7a63\"></rect>"
           << "<rect x=\"" << (bar_x + h2d_width + kernel_width) << "\" y=\"" << bar_y << "\" width=\""
           << d2h_width << "\" height=\"" << bar_height << "\" rx=\"" << d2h_radius
           << "\" fill=\"#c8791b\"></rect>"
           << "<text x=\"20\" y=\"26\" font-size=\"13\" fill=\"#4f5b54\">Доля медианы по этапам</text></svg>";
    stream << "<div class=\"legend\"><span><i class=\"swatch\" style=\"background:#1f6fb3\"></i>Хост → GPU "
           << html_escape(format_ms(h2d)) << " (" << html_escape(format_percent(h2d / total))
           << ")</span><span><i class=\"swatch\" style=\"background:#1d7a63\"></i>Ядро "
           << html_escape(format_ms(kernel)) << " (" << html_escape(format_percent(kernel / total))
           << ")</span><span><i class=\"swatch\" style=\"background:#c8791b\"></i>GPU → хост "
           << html_escape(format_ms(d2h)) << " (" << html_escape(format_percent(d2h / total))
           << ")</span></div></section>";
}

inline void append_timing_rows(std::ostream& stream, const benchmark::BenchmarkResult& result) {
    const std::pair<std::string_view, const core::RunStats*> rows[] = {
        {"Хост → GPU", &result.h2d_stats},
        {"Ядро", &result.kernel_stats},
        {"GPU → хост", &result.d2h_stats},
        {"Итого", &result.total_stats},
    };

    for (const auto& [label, stats] : rows) {
        stream << "<tr><td>" << html_escape(label) << "</td><td>" << html_escape(format_ms(stats->mean_ms))
               << "</td><td>" << html_escape(format_ms(stats->median_ms)) << "</td><td>"
               << html_escape(format_ms(stats->p95_ms)) << "</td><td>"
               << html_escape(format_ms(stats->ci95_low)) << "</td><td>"
               << html_escape(format_ms(stats->ci95_high)) << "</td><td>"
               << html_escape(format_scalar(stats->cv)) << "</td></tr>";
    }
}

inline void append_timing_section(std::ostream& stream,
                                  const pipeline::PipelineReport& report,
                                  std::string_view section_id) {
    const TimingSelection timing = select_primary_timing(report);
    if (timing.result == nullptr) {
        append_empty_panel(stream,
                           "Время",
                           "Сводка по времени",
                           "Для этого отчета нет временных данных.",
                           section_id);
        return;
    }

    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Время", "Сводка по времени", timing.label);
    stream << "<div class=\"table-wrap\"><table><thead><tr><th>Этап</th><th>Mean</th><th>Median</th><th>P95</th>"
              "<th>CI95 low</th><th>CI95 high</th><th>CV</th></tr></thead><tbody>";
    append_timing_rows(stream, *timing.result);
    stream << "</tbody></table></div></section>";
}

inline void append_fingerprint_summary(std::ostream& stream, const analysis::KernelFingerprint& fingerprint) {
    stream << "<p class=\"raw-note\">Профиль: теоретическая заполняемость "
           << html_escape(format_optional_percent(fingerprint.occupancy, fingerprint.has_occupancy))
           << ", утилизация пропускной способности "
           << html_escape(format_optional_percent(
                  fingerprint.bandwidth_utilization, fingerprint.has_bandwidth_utilization))
           << ", отношение передачи/вычисления "
           << html_escape(format_scalar(fingerprint.transfer_compute_ratio)) << ", регистры "
           << html_escape(format_optional_int(fingerprint.num_regs, fingerprint.has_kernel_attributes))
           << ", локальная память "
           << html_escape(
                  format_optional_bytes(fingerprint.local_size_bytes, fingerprint.has_kernel_attributes))
           << ".</p>";
}

inline void append_recommendations_section(std::ostream& stream,
                                           const pipeline::PipelineReport& report,
                                           std::string_view section_id) {
    stream << "<section class=\"panel\" id=\"" << html_escape(section_id) << "\">";
    append_section_heading(stream, "Диагностика", "Рекомендации");

    if (!report.diagnose_enabled()) {
        stream << "<p class=\"muted\">Диагностика для этого пайплайна не запускалась.</p></section>";
        return;
    }

    if (report.fingerprint().has_value()) {
        append_fingerprint_summary(stream, *report.fingerprint());
    }

    if (report.recommendations().empty()) {
        stream << "<p class=\"muted\">Рекомендации не сгенерированы.</p></section>";
        return;
    }

    stream << "<div class=\"rec-list\">";
    for (const analysis::Recommendation& recommendation : report.recommendations()) {
        stream << "<article class=\"rec-card " << html_escape(severity_class(recommendation.severity))
               << "\"><span class=\"tag\">" << html_escape(severity_label(recommendation.severity)) << "</span><h3>"
               << html_escape(recommendation.summary) << "</h3><p><strong>Тег:</strong> <code>"
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
               << "\"><div class=\"article-header\"><p class=\"eyebrow\">Раздел ядра</p><h2>"
               << html_escape(report.kernel_name()) << "</h2><p class=\"meta-line\">"
               << html_escape(device_label(report.device_id())) << "</p></div>";
    } else {
        append_pipeline_hero(stream, report, "отчет cuda_test по pipeline", "h1");
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
    stream << "<section class=\"panel\"><div class=\"section-heading\"><p class=\"eyebrow\">Сырые данные</p><h2>"
              "Встроенный машиночитаемый JSON</h2><p class=\"muted\">Полное содержимое отчета хранится внутри "
              "встроенного тега <code>application/json</code> для последующей автоматической обработки.</p></div></section>";
}

inline void append_suite_hero(std::ostream& stream, const pipeline::SuiteReport& report) {
    stream << "<header class=\"hero\"><p class=\"eyebrow\">сводный отчет cuda_test</p><h1>"
           << html_escape(report.name().empty() ? std::string("Безымянный набор") : report.name())
           << "</h1><p class=\"meta-line\">" << report.reports().size() << " отчет(ов) по ядрам | Сформировано "
           << html_escape(generated_timestamp()) << " | cuda_test v" << html_escape(version_string())
           << "</p><div class=\"badge-row\"><span class=\"pill " << (report.all_passed() ? "pass" : "fail")
           << "\">" << (report.all_passed() ? "Все отчеты пройдены" : "Есть отчеты с ошибками")
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
                           "Обзор",
                           "Сводка по ядрам",
                           "Время лучшей конфигурации берется из автотюнинга, если он запускался; иначе показывается базовый замер.");

    if (report.reports().empty()) {
        stream << "<p class=\"muted\">Ядра не запускались.</p></section>";
        return;
    }

    stream << "<div class=\"table-wrap\"><table><thead><tr><th>Ядро</th><th>Статус</th><th>Лучшая конфигурация</th>"
              "<th>Медиана базового замера</th><th>Лучшая медиана</th><th>Эффект относительно базового замера</th><th>Находки</th></tr></thead><tbody>";

    for (const pipeline::PipelineReport& kernel_report : report.reports()) {
        const std::string anchor = "kernel-" + sanitize_anchor(kernel_report.kernel_name());
        const std::optional<double> baseline = suite_baseline_median(kernel_report);
        const std::optional<double> best = suite_best_median(kernel_report);

        std::string best_config = "базовая конфигурация";
        if (kernel_report.autotune_result().has_value()) {
            best_config = launch_config_label(kernel_report.autotune_result()->best);
        } else {
            best_config = "базовая конфигурация";
        }

        RelativeEffectSummary relative_effect;
        if (baseline.has_value() && best.has_value() && *best > 0.0 && kernel_report.autotune_result().has_value()) {
            relative_effect = summarize_relative_effect(kernel_report.benchmark_result()->kernel_stats,
                                                        kernel_report.autotune_result()->stats);
        }

        stream << "<tr><td><a href=\"#" << html_escape(anchor) << "\">" << html_escape(kernel_report.kernel_name())
               << "</a></td><td>" << (kernel_report.passed() ? "Пройден" : "Ошибка") << "</td><td>"
               << html_escape(best_config) << "</td><td>"
               << html_escape(baseline.has_value() ? format_ms(*baseline) : std::string("н/д")) << "</td><td>"
               << html_escape(best.has_value() ? format_ms(*best) : std::string("н/д")) << "</td><td>"
               << html_escape(relative_effect.value) << "</td><td>" << kernel_report.recommendations().size()
               << "</td></tr>";
    }

    stream << "</tbody></table></div></section>";
}

inline std::string pipeline_raw_json(const pipeline::PipelineReport& report) {
    std::ostringstream stream;
    write_pipeline_report_json(stream, report);
    return stream.str();
}

inline std::string suite_raw_json(const pipeline::SuiteReport& report) {
    std::ostringstream stream;
    write_suite_report_json(stream, report);
    return stream.str();
}

inline std::string render_pipeline_html(const pipeline::PipelineReport& report) {
    std::ostringstream stream;
    const std::string title = "cuda_test Отчет: " + report.kernel_name();
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
        "cuda_test Сводный отчет: " + (report.name().empty() ? std::string("безымянный") : report.name());

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

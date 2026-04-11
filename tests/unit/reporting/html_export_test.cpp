#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cuda_test/reporting/html_export.hpp"

namespace cuda_test::reporting {
namespace {

core::RunStats make_stats(double mean_ms,
                          double median_ms,
                          double p95_ms,
                          double ci95_low,
                          double ci95_high,
                          double cv) {
    core::RunStats stats;
    stats.mean_ms = mean_ms;
    stats.median_ms = median_ms;
    stats.p95_ms = p95_ms;
    stats.ci95_low = ci95_low;
    stats.ci95_high = ci95_high;
    stats.cv = cv;
    return stats;
}

benchmark::BenchmarkResult make_benchmark_result(double kernel_median_ms = 2.4) {
    benchmark::BenchmarkResult result;
    result.samples = {
        {0.5, kernel_median_ms, 0.3, 0.5 + kernel_median_ms + 0.3},
        {0.6, kernel_median_ms + 0.1, 0.4, 1.0 + kernel_median_ms},
    };
    result.h2d_stats = make_stats(0.55, 0.55, 0.6, 0.5, 0.6, 0.04);
    result.kernel_stats = make_stats(kernel_median_ms + 0.05,
                                     kernel_median_ms,
                                     kernel_median_ms + 0.25,
                                     kernel_median_ms - 0.1,
                                     kernel_median_ms + 0.1,
                                     0.06);
    result.d2h_stats = make_stats(0.35, 0.35, 0.4, 0.3, 0.4, 0.05);
    result.total_stats = make_stats(0.95 + kernel_median_ms,
                                    0.9 + kernel_median_ms,
                                    1.0 + kernel_median_ms,
                                    0.8 + kernel_median_ms,
                                    1.1 + kernel_median_ms,
                                    0.05);
    return result;
}

autotune::CandidateRecord make_candidate(unsigned int block_x,
                                         unsigned int grid_x,
                                         int wave_multiplier,
                                         double kernel_median_ms,
                                         std::size_t shared_mem = 0U) {
    autotune::CandidateRecord candidate;
    candidate.config.block = dim3(block_x, 1, 1);
    candidate.config.grid = dim3(grid_x, 1, 1);
    candidate.config.shared_mem = shared_mem;
    candidate.grid_wave_multiplier = wave_multiplier;
    candidate.benchmark = make_benchmark_result(kernel_median_ms);
    return candidate;
}

autotune::AutoTuneResult make_autotune_result() {
    autotune::AutoTuneResult result;
    result.all_candidates.push_back(make_candidate(64U, 8U, 1, 3.1));
    result.all_candidates.push_back(make_candidate(128U, 4U, 2, 1.8, 64U));
    result.all_candidates.push_back(make_candidate(256U, 2U, 1, 2.2));
    result.best = result.all_candidates[1].config;
    result.stats = result.all_candidates[1].benchmark.kernel_stats;
    result.reason = "Выбрана конфигурация: блок=128, сетка=4, медиана ядра=1.8 потому что у нее минимальная медиана ядра.";
    return result;
}

analysis::KernelFingerprint make_fingerprint() {
    analysis::KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = 0.4;
    fingerprint.occupancy = 0.68;
    fingerprint.has_occupancy = true;
    fingerprint.bandwidth_utilization = 0.82;
    fingerprint.has_bandwidth_utilization = true;
    fingerprint.cv = 0.04;
    fingerprint.block_sensitivity = 1.42;
    fingerprint.scaling_exponent = 1.05;
    fingerprint.has_scaling_exponent = true;
    fingerprint.num_regs = 48;
    fingerprint.local_size_bytes = 16U;
    fingerprint.shared_size_bytes = 64U;
    fingerprint.has_kernel_attributes = true;
    return fingerprint;
}

pipeline::PipelineReport make_full_report(std::string kernel_name = "test_kernel") {
    return pipeline::detail::PipelineReportBuilder()
        .kernel_name(std::move(kernel_name))
        .device_id(2)
        .correctness(true, true)
        .benchmark(make_benchmark_result())
        .autotune(make_autotune_result())
        .diagnose(true,
                  make_fingerprint(),
                  {{"unstable_timing", "warning", "Разброс времени слишком велик.", "Уменьшите фоновую нагрузку на GPU."},
                   {"bandwidth_bound",
                    "info",
                    "Ядро упирается в пропускную способность памяти.",
                    "Улучшите коалесцирование и использование разделяемой памяти."}})
        .build();
}

pipeline::PipelineReport make_failed_report(std::string kernel_name = "failing_kernel") {
    return pipeline::detail::PipelineReportBuilder()
        .kernel_name(std::move(kernel_name))
        .device_id(1)
        .correctness(true, false)
        .build();
}

pipeline::PipelineReport make_minimal_report(std::string kernel_name = "minimal_kernel") {
    return pipeline::detail::PipelineReportBuilder().kernel_name(std::move(kernel_name)).device_id(0).build();
}

pipeline::SuiteReport make_suite_report() {
    return pipeline::detail::SuiteReportBuilder()
        .name("suite_alpha")
        .add_report(make_full_report("alpha_kernel"))
        .add_report(make_failed_report("beta_kernel"))
        .build();
}

std::filesystem::path make_output_path(const std::string& filename) {
    const std::string build_tag =
        std::to_string(std::hash<std::string>{}(std::filesystem::current_path().string()));
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "cuda_test_html_export_unit" / build_tag;
    std::filesystem::create_directories(root);
    return root / filename;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::size_t count_occurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0U;
    std::size_t position = 0U;

    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }

    return count;
}

std::string normalize_generated_timestamps(std::string text) {
    const std::string marker = "Сформировано ";
    const std::string suffix = " | cuda_test v";
    std::size_t search_from = 0U;

    while (true) {
        const std::size_t marker_pos = text.find(marker, search_from);
        if (marker_pos == std::string::npos) {
            break;
        }

        const std::size_t value_begin = marker_pos + marker.size();
        const std::size_t value_end = text.find(suffix, value_begin);
        if (value_end == std::string::npos) {
            break;
        }

        text.replace(value_begin, value_end - value_begin, "<normalized>");
        search_from = value_begin + 12U;
    }

    return text;
}

TEST(HtmlExportTest, PipelineHtmlExportWritesFullStandaloneDocument) {
    const std::filesystem::path path = make_output_path("pipeline_full.html");
    const pipeline::PipelineReport report = make_full_report();

    export_html(path, report);

    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 1024U);

    const std::string text = read_text(path);
    EXPECT_EQ(text.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(text.find("<title>cuda_test Отчет: test_kernel</title>"), std::string::npos);
    EXPECT_NE(text.find(">test_kernel</h1>"), std::string::npos);
    EXPECT_NE(text.find("Пройден"), std::string::npos);
    EXPECT_NE(text.find("Перебор кандидатов"), std::string::npos);
    EXPECT_NE(text.find("64x1x1"), std::string::npos);
    EXPECT_NE(text.find("128x1x1"), std::string::npos);
    EXPECT_NE(text.find("winner-row"), std::string::npos);
    EXPECT_NE(text.find("Разбивка по этапам"), std::string::npos);
    EXPECT_NE(text.find("Сводка по времени"), std::string::npos);
    EXPECT_NE(text.find("Рекомендации"), std::string::npos);
    EXPECT_NE(text.find("sev-warning"), std::string::npos);
    EXPECT_NE(text.find("sev-info"), std::string::npos);
    EXPECT_NE(text.find("<script type=\"application/json\" id=\"raw-data\">"), std::string::npos);
    EXPECT_NE(text.find("\"kernel_name\":\"test_kernel\""), std::string::npos);
    EXPECT_NE(text.find("ускорение 1.33333x"), std::string::npos);
    EXPECT_NE(text.find("1.80000 ms"), std::string::npos);
    EXPECT_NE(text.find("<svg"), std::string::npos);
    EXPECT_EQ(text.find("http://"), std::string::npos);
    EXPECT_EQ(text.find("https://"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportShowsFailedCorrectnessBadge) {
    const std::filesystem::path path = make_output_path("pipeline_failed.html");

    export_html(path, make_failed_report());

    const std::string text = read_text(path);
    EXPECT_NE(text.find("Проверка не пройдена"), std::string::npos);
    EXPECT_NE(text.find(">Ошибка<"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportEscapesVisibleTextAndScriptSensitiveContent) {
    pipeline::PipelineReport report = make_full_report("test</script>&kernel<demo>");
    report = pipeline::detail::PipelineReportBuilder()
                 .kernel_name(report.kernel_name())
                 .device_id(report.device_id())
                 .correctness(report.correctness_enabled(), report.correctness_passed())
                 .benchmark(report.benchmark_result(), report.benchmark_enabled())
                 .autotune(report.autotune_result(), report.autotune_enabled())
                 .diagnose(true,
                           report.fingerprint(),
                           {{"unstable_timing",
                             "warning",
                             "Разброс времени слишком велик.",
                             "Не вставляйте </script> в копируемые фрагменты."},
                            {"bandwidth_bound",
                             "info",
                             "Ядро упирается в пропускную способность памяти.",
                             "Улучшите коалесцирование и использование разделяемой памяти."}})
                 .build();
    const std::filesystem::path path = make_output_path("pipeline_escaped.html");

    export_html(path, report);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("test&lt;/script&gt;&amp;kernel&lt;demo&gt;"), std::string::npos);
    EXPECT_NE(text.find("Не вставляйте &lt;/script&gt; в копируемые фрагменты."), std::string::npos);
    EXPECT_NE(text.find("\\u003C/script\\u003E"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportHandlesMinimalReport) {
    const std::filesystem::path path = make_output_path("pipeline_minimal.html");

    export_html(path, make_minimal_report());

    const std::string text = read_text(path);
    EXPECT_EQ(text.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(text.find("Для этого отчета нет данных автотюнинга."), std::string::npos);
    EXPECT_NE(text.find("Для этого отчета нет временных данных."), std::string::npos);
    EXPECT_NE(text.find("Диагностика для этого пайплайна не запускалась."), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportShowsNdForUnavailableRuntimeMetrics) {
    analysis::KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = 1.25;
    fingerprint.cv = 0.08;
    fingerprint.block_sensitivity = 1.1;

    const pipeline::PipelineReport report = pipeline::detail::PipelineReportBuilder()
                                                .kernel_name("partial_metrics_kernel")
                                                .device_id(0)
                                                .diagnose(true, fingerprint)
                                                .build();
    const std::filesystem::path path = make_output_path("pipeline_partial_metrics.html");

    export_html(path, report);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("теоретическая заполняемость н/д"), std::string::npos);
    EXPECT_NE(text.find("утилизация пропускной способности н/д"), std::string::npos);
    EXPECT_NE(text.find("регистры н/д"), std::string::npos);
    EXPECT_NE(text.find("локальная память н/д"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportShowsSlowdownWhenMedianGapIsLargeDespiteOverlap) {
    benchmark::BenchmarkResult benchmark_result = make_benchmark_result(0.05);
    benchmark_result.kernel_stats.ci95_low = 0.01;
    benchmark_result.kernel_stats.ci95_high = 0.40;

    autotune::AutoTuneResult autotune_result = make_autotune_result();
    autotune_result.stats = make_stats(0.31, 0.30, 0.45, 0.25, 0.45, 0.20);
    autotune_result.best = autotune_result.all_candidates[1].config;
    autotune_result.all_candidates[1].benchmark.kernel_stats = autotune_result.stats;

    const pipeline::PipelineReport report = pipeline::detail::PipelineReportBuilder()
                                                .kernel_name("large_gap_kernel")
                                                .device_id(0)
                                                .correctness(true, true)
                                                .benchmark(benchmark_result)
                                                .autotune(autotune_result)
                                                .build();
    const std::filesystem::path path = make_output_path("pipeline_large_gap.html");

    export_html(path, report);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("замедление 6.00000x"), std::string::npos);
    EXPECT_EQ(text.find("в пределах шума"), std::string::npos);
}

TEST(HtmlExportTest, PipelineMemberDelegationMatchesDirectExport) {
    const pipeline::PipelineReport report = make_full_report("delegate_kernel");
    const std::filesystem::path direct_path = make_output_path("pipeline_direct.html");
    const std::filesystem::path member_path = make_output_path("pipeline_member.html");

    export_html(direct_path, report);
    report.to_html(member_path);

    EXPECT_EQ(normalize_generated_timestamps(read_text(direct_path)),
              normalize_generated_timestamps(read_text(member_path)));
}

TEST(HtmlExportTest, SuiteHtmlExportWritesSummaryAndKernelSections) {
    const std::filesystem::path path = make_output_path("suite_full.html");
    const pipeline::SuiteReport suite = make_suite_report();

    export_html(path, suite);

    const std::string text = read_text(path);
    EXPECT_EQ(text.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(text.find("suite_alpha"), std::string::npos);
    EXPECT_NE(text.find("Сводка по ядрам"), std::string::npos);
    EXPECT_NE(text.find("alpha_kernel"), std::string::npos);
    EXPECT_NE(text.find("beta_kernel"), std::string::npos);
    EXPECT_EQ(count_occurrences(text, "class=\"pipeline-article\""), 2U);
    EXPECT_NE(text.find("<script type=\"application/json\" id=\"suite-raw-data\">"), std::string::npos);
    EXPECT_NE(text.find("Эффект относительно базового замера"), std::string::npos);
    EXPECT_NE(text.find("\"report_count\":2"), std::string::npos);
}

TEST(HtmlExportTest, SuiteHtmlExportHandlesEmptySuite) {
    const pipeline::SuiteReport suite = pipeline::detail::SuiteReportBuilder().name("empty_suite").build();
    const std::filesystem::path path = make_output_path("suite_empty.html");

    export_html(path, suite);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("Ядра не запускались."), std::string::npos);
    EXPECT_NE(text.find("empty_suite"), std::string::npos);
}

TEST(HtmlExportTest, SuiteMemberDelegationMatchesDirectExport) {
    const pipeline::SuiteReport suite = make_suite_report();
    const std::filesystem::path direct_path = make_output_path("suite_direct.html");
    const std::filesystem::path member_path = make_output_path("suite_member.html");

    export_html(direct_path, suite);
    suite.to_html(member_path);

    EXPECT_EQ(normalize_generated_timestamps(read_text(direct_path)),
              normalize_generated_timestamps(read_text(member_path)));
}

TEST(HtmlExportTest, HtmlExportCreatesParentDirectories) {
    const std::filesystem::path path =
        make_output_path("nested") / "deeper" / "pipeline_parent_create.html";

    export_html(path, make_full_report("nested_kernel"));

    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST(HtmlExportTest, InvalidHtmlExportPathRaisesError) {
    const std::filesystem::path directory_path = make_output_path("html_directory_as_file");
    std::filesystem::create_directories(directory_path);

    EXPECT_THROW(export_html(directory_path, make_full_report()), std::runtime_error);
    EXPECT_THROW(export_html(directory_path, make_suite_report()), std::runtime_error);
}

} // namespace
} // namespace cuda_test::reporting

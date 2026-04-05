#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#define private public
#include "cuda_test/reporting/html_export.hpp"
#undef private

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
    result.reason = "Selected block=128 because it had the lowest median.";
    return result;
}

analysis::KernelFingerprint make_fingerprint() {
    analysis::KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = 0.4;
    fingerprint.occupancy = 0.68;
    fingerprint.bandwidth_utilization = 0.82;
    fingerprint.cv = 0.04;
    fingerprint.block_sensitivity = 1.42;
    fingerprint.scaling_exponent = 1.05;
    fingerprint.num_regs = 48;
    fingerprint.local_size_bytes = 16U;
    fingerprint.shared_size_bytes = 64U;
    return fingerprint;
}

pipeline::PipelineReport make_full_report(std::string kernel_name = "test_kernel") {
    pipeline::PipelineReport report;
    report.kernel_name_ = std::move(kernel_name);
    report.device_id_ = 2;
    report.correctness_enabled_ = true;
    report.correctness_passed_ = true;
    report.benchmark_enabled_ = true;
    report.autotune_enabled_ = true;
    report.diagnose_enabled_ = true;
    report.benchmark_result_ = make_benchmark_result();
    report.autotune_result_ = make_autotune_result();
    report.fingerprint_ = make_fingerprint();
    report.recommendations_ = {
        {"unstable_timing", "warning", "Timing variation is high.", "Reduce background GPU work."},
        {"bandwidth_bound", "info", "The kernel is bandwidth limited.", "Improve coalescing and shared memory use."},
    };
    return report;
}

pipeline::PipelineReport make_failed_report(std::string kernel_name = "failing_kernel") {
    pipeline::PipelineReport report;
    report.kernel_name_ = std::move(kernel_name);
    report.device_id_ = 1;
    report.correctness_enabled_ = true;
    report.correctness_passed_ = false;
    return report;
}

pipeline::PipelineReport make_minimal_report(std::string kernel_name = "minimal_kernel") {
    pipeline::PipelineReport report;
    report.kernel_name_ = std::move(kernel_name);
    report.device_id_ = 0;
    return report;
}

pipeline::SuiteReport make_suite_report() {
    pipeline::SuiteReport suite;
    suite.name_ = "suite_alpha";
    suite.reports_.push_back(make_full_report("alpha_kernel"));
    suite.reports_.push_back(make_failed_report("beta_kernel"));
    return suite;
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

TEST(HtmlExportTest, PipelineHtmlExportWritesFullStandaloneDocument) {
    const std::filesystem::path path = make_output_path("pipeline_full.html");
    const pipeline::PipelineReport report = make_full_report();

    export_html(path, report);

    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 1024U);

    const std::string text = read_text(path);
    EXPECT_EQ(text.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(text.find("<title>cuda_test Report: test_kernel</title>"), std::string::npos);
    EXPECT_NE(text.find(">test_kernel</h1>"), std::string::npos);
    EXPECT_NE(text.find("Passed"), std::string::npos);
    EXPECT_NE(text.find("Candidate sweep"), std::string::npos);
    EXPECT_NE(text.find("64x1x1"), std::string::npos);
    EXPECT_NE(text.find("128x1x1"), std::string::npos);
    EXPECT_NE(text.find("winner-row"), std::string::npos);
    EXPECT_NE(text.find("Stage breakdown"), std::string::npos);
    EXPECT_NE(text.find("Timing summary"), std::string::npos);
    EXPECT_NE(text.find("Recommendations"), std::string::npos);
    EXPECT_NE(text.find("sev-warning"), std::string::npos);
    EXPECT_NE(text.find("sev-info"), std::string::npos);
    EXPECT_NE(text.find("<script type=\"application/json\" id=\"raw-data\">"), std::string::npos);
    EXPECT_NE(text.find("\"kernel_name\":\"test_kernel\""), std::string::npos);
    EXPECT_NE(text.find("<svg"), std::string::npos);
    EXPECT_EQ(text.find("http://"), std::string::npos);
    EXPECT_EQ(text.find("https://"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportShowsFailedCorrectnessBadge) {
    const std::filesystem::path path = make_output_path("pipeline_failed.html");

    export_html(path, make_failed_report());

    const std::string text = read_text(path);
    EXPECT_NE(text.find("Correctness failed"), std::string::npos);
    EXPECT_NE(text.find(">Failed<"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportEscapesVisibleTextAndScriptSensitiveContent) {
    pipeline::PipelineReport report = make_full_report("test</script>&kernel<demo>");
    report.recommendations_[0].suggestion = "Avoid </script> in copied snippets.";
    const std::filesystem::path path = make_output_path("pipeline_escaped.html");

    export_html(path, report);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("test&lt;/script&gt;&amp;kernel&lt;demo&gt;"), std::string::npos);
    EXPECT_NE(text.find("Avoid &lt;/script&gt; in copied snippets."), std::string::npos);
    EXPECT_NE(text.find("\\u003C/script\\u003E"), std::string::npos);
}

TEST(HtmlExportTest, PipelineHtmlExportHandlesMinimalReport) {
    const std::filesystem::path path = make_output_path("pipeline_minimal.html");

    export_html(path, make_minimal_report());

    const std::string text = read_text(path);
    EXPECT_EQ(text.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(text.find("No autotune data available for this report."), std::string::npos);
    EXPECT_NE(text.find("No timing data available for this report."), std::string::npos);
    EXPECT_NE(text.find("Diagnostics were not run for this pipeline."), std::string::npos);
}

TEST(HtmlExportTest, PipelineMemberDelegationMatchesDirectExport) {
    const pipeline::PipelineReport report = make_full_report("delegate_kernel");
    const std::filesystem::path direct_path = make_output_path("pipeline_direct.html");
    const std::filesystem::path member_path = make_output_path("pipeline_member.html");

    export_html(direct_path, report);
    report.to_html(member_path);

    EXPECT_EQ(read_text(direct_path), read_text(member_path));
}

TEST(HtmlExportTest, SuiteHtmlExportWritesSummaryAndKernelSections) {
    const std::filesystem::path path = make_output_path("suite_full.html");
    const pipeline::SuiteReport suite = make_suite_report();

    export_html(path, suite);

    const std::string text = read_text(path);
    EXPECT_EQ(text.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(text.find("suite_alpha"), std::string::npos);
    EXPECT_NE(text.find("Kernel summary"), std::string::npos);
    EXPECT_NE(text.find("alpha_kernel"), std::string::npos);
    EXPECT_NE(text.find("beta_kernel"), std::string::npos);
    EXPECT_EQ(count_occurrences(text, "class=\"pipeline-article\""), 2U);
    EXPECT_NE(text.find("<script type=\"application/json\" id=\"suite-raw-data\">"), std::string::npos);
    EXPECT_NE(text.find("\"report_count\":2"), std::string::npos);
}

TEST(HtmlExportTest, SuiteHtmlExportHandlesEmptySuite) {
    pipeline::SuiteReport suite;
    suite.name_ = "empty_suite";
    const std::filesystem::path path = make_output_path("suite_empty.html");

    export_html(path, suite);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("No kernels tested."), std::string::npos);
    EXPECT_NE(text.find("empty_suite"), std::string::npos);
}

TEST(HtmlExportTest, SuiteMemberDelegationMatchesDirectExport) {
    const pipeline::SuiteReport suite = make_suite_report();
    const std::filesystem::path direct_path = make_output_path("suite_direct.html");
    const std::filesystem::path member_path = make_output_path("suite_member.html");

    export_html(direct_path, suite);
    suite.to_html(member_path);

    EXPECT_EQ(read_text(direct_path), read_text(member_path));
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

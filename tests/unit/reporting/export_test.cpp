#include "cuda_test/reporting/export.hpp"
#include "cuda_test/pipeline/pipeline.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace cuda_test::reporting {
namespace {

benchmark::BenchmarkResult make_benchmark_result_fixture() {
    std::vector<core::ProfilingBreakdown> samples = {
        {1.0, 1.0, 0.5, 2.5},
        {1.0, 2.0, 0.5, 3.5},
        {1.0, 3.0, 0.5, 4.5},
        {1.0, 4.0, 0.5, 5.5},
        {1.0, 5.0, 0.5, 6.5},
    };

    std::size_t index = 0;
    const benchmark::BenchmarkRunner runner(benchmark::BenchmarkConfig{0, static_cast<int>(samples.size())});
    return runner.run([&samples, &index]() {
        return samples.at(index++);
    });
}

autotune::CandidateRecord make_candidate(unsigned int block_x,
                                         unsigned int grid_x,
                                         int wave_multiplier,
                                         const core::RunStats& h2d_stats,
                                         const core::RunStats& kernel_stats,
                                         const core::RunStats& d2h_stats,
                                         const core::RunStats& total_stats) {
    autotune::CandidateRecord candidate;
    candidate.config.block = dim3(block_x, 1, 1);
    candidate.config.grid = dim3(grid_x, 1, 1);
    candidate.grid_wave_multiplier = wave_multiplier;
    candidate.benchmark.h2d_stats = h2d_stats;
    candidate.benchmark.kernel_stats = kernel_stats;
    candidate.benchmark.d2h_stats = d2h_stats;
    candidate.benchmark.total_stats = total_stats;
    return candidate;
}

autotune::AutoTuneResult make_result_fixture() {
    const core::RunStats h2d_first{1.0, 1.0, 1.0, 1.0, 1.0, 0.0};
    const core::RunStats kernel_first{5.0, 5.0, 5.0, 5.0, 5.0, 0.0};
    const core::RunStats d2h_first{2.0, 2.0, 2.0, 2.0, 2.0, 0.0};
    const core::RunStats total_first{8.0, 8.0, 8.0, 8.0, 8.0, 0.0};

    const core::RunStats h2d_second{0.5, 0.5, 0.5, 0.5, 0.5, 0.0};
    const core::RunStats kernel_second{1.5, 1.5, 1.5, 1.5, 1.5, 0.0};
    const core::RunStats d2h_second{0.7, 0.7, 0.7, 0.7, 0.7, 0.0};
    const core::RunStats total_second{2.7, 2.7, 2.7, 2.7, 2.7, 0.0};

    autotune::AutoTuneResult result;
    result.all_candidates.push_back(
        make_candidate(64U, 4U, 1, h2d_first, kernel_first, d2h_first, total_first));
    result.all_candidates.push_back(
        make_candidate(128U, 2U, 2, h2d_second, kernel_second, d2h_second, total_second));
    result.best = result.all_candidates[1].config;
    result.stats = result.all_candidates[1].benchmark.kernel_stats;
    result.reason = "Selected block=128, grid=2 because it had the lowest median_ms.";
    return result;
}

std::filesystem::path make_output_path(const std::string& filename) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "cuda_test_reporting_unit";
    std::filesystem::create_directories(root);
    return root / filename;
}

std::vector<std::string> read_lines(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> split_csv_row(const std::string& row) {
    std::vector<std::string> fields;
    std::stringstream stream(row);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

pipeline::KernelDescriptor make_no_stage_descriptor(std::string name, std::size_t problem_size = 16U) {
    return describe_kernel(std::move(name))
        .problem_size(problem_size)
        .inputs([problem_size]() { return std::make_tuple(std::vector<int>(problem_size, 1)); })
        .expected([problem_size]() { return std::vector<int>(problem_size, 1); })
        .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
        .build();
}

pipeline::PipelineReport make_no_stage_report() {
    return cuda_test::make_pipeline(make_no_stage_descriptor("report_pipeline")).device(4).run();
}

TEST(ExportTest, BenchmarkCsvExportWritesHeaderAndSummaryRow) {
    const std::filesystem::path path = make_output_path("benchmark_result.csv");
    const benchmark::BenchmarkResult result = make_benchmark_result_fixture();

    export_csv(path, result);

    const std::vector<std::string> lines = read_lines(path);
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0],
              "h2d_mean_ms,h2d_median_ms,h2d_p95_ms,h2d_ci95_low,h2d_ci95_high,h2d_cv,"
              "kernel_mean_ms,kernel_median_ms,kernel_p95_ms,kernel_ci95_low,kernel_ci95_high,kernel_cv,"
              "d2h_mean_ms,d2h_median_ms,d2h_p95_ms,d2h_ci95_low,d2h_ci95_high,d2h_cv,"
              "total_mean_ms,total_median_ms,total_p95_ms,total_ci95_low,total_ci95_high,total_cv");

    const std::vector<std::string> row = split_csv_row(lines[1]);
    ASSERT_EQ(row.size(), 24U);
    EXPECT_NEAR(std::stod(row[6]), 3.0, 1e-9);
    EXPECT_NEAR(std::stod(row[7]), 3.0, 1e-9);
    EXPECT_NEAR(std::stod(row[18]), 4.5, 1e-9);
}

TEST(ExportTest, BenchmarkJsonExportWritesStageStats) {
    const std::filesystem::path path = make_output_path("benchmark_result.json");
    const benchmark::BenchmarkResult result = make_benchmark_result_fixture();

    export_json(path, result);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("\"sample_count\":5"), std::string::npos);
    EXPECT_NE(text.find("\"h2d_stats\""), std::string::npos);
    EXPECT_NE(text.find("\"kernel_stats\""), std::string::npos);
    EXPECT_NE(text.find("\"median_ms\":3"), std::string::npos);
}

TEST(ExportTest, BenchmarkJsonExportHandlesEmptyResult) {
    const std::filesystem::path path = make_output_path("benchmark_empty.json");

    export_json(path, benchmark::BenchmarkResult{});

    const std::string text = read_text(path);
    EXPECT_NE(text.find("\"sample_count\":0"), std::string::npos);
    EXPECT_NE(text.find("\"mean_ms\":0"), std::string::npos);
}

TEST(ExportTest, PipelineJsonExportUsesEnvelopeAndConditionalSections) {
    const std::filesystem::path path = make_output_path("pipeline_report.json");
    const pipeline::PipelineReport report = make_no_stage_report();

    export_json(path, report);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("\"kernel_name\":\"report_pipeline\""), std::string::npos);
    EXPECT_NE(text.find("\"device_id\":4"), std::string::npos);
    EXPECT_NE(text.find("\"correctness\":{\"enabled\":false,\"passed\":false}"), std::string::npos);
    EXPECT_EQ(text.find("\"benchmark\":"), std::string::npos);
    EXPECT_EQ(text.find("\"autotune\":"), std::string::npos);
}

TEST(ExportTest, PipelineCsvExportWritesSummaryWithEmptyOptionalColumns) {
    const std::filesystem::path path = make_output_path("pipeline_report.csv");
    const pipeline::PipelineReport report = make_no_stage_report();

    export_csv(path, report);

    const std::vector<std::string> lines = read_lines(path);
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0],
              "kernel_name,device_id,correctness_enabled,correctness_passed,passed,benchmark_enabled,"
              "autotune_enabled,benchmark_sample_count,benchmark_kernel_mean_ms,benchmark_kernel_median_ms,"
              "benchmark_kernel_p95_ms,benchmark_kernel_cv,benchmark_total_mean_ms,"
              "autotune_candidate_count,autotune_best_grid_x,autotune_best_block_x,"
              "autotune_best_shared_mem,autotune_best_device_id,autotune_kernel_mean_ms,"
              "autotune_kernel_median_ms,autotune_kernel_p95_ms,autotune_kernel_cv,autotune_reason");

    const std::vector<std::string> row = split_csv_row(lines[1]);
    ASSERT_EQ(row.size(), 23U);
    EXPECT_EQ(row[0], "\"report_pipeline\"");
    EXPECT_EQ(row[1], "4");
    EXPECT_EQ(row[2], "false");
    EXPECT_EQ(row[4], "true");
    EXPECT_TRUE(row[7].empty());
    EXPECT_TRUE(row[13].empty());
}

TEST(ExportTest, PipelineMemberDelegationMatchesReportingOutput) {
    const std::filesystem::path direct_json = make_output_path("pipeline_direct.json");
    const std::filesystem::path member_json = make_output_path("pipeline_member.json");
    const std::filesystem::path direct_csv = make_output_path("pipeline_direct.csv");
    const std::filesystem::path member_csv = make_output_path("pipeline_member.csv");
    const pipeline::PipelineReport report = make_no_stage_report();

    export_json(direct_json, report);
    report.to_json(member_json);
    export_csv(direct_csv, report);
    report.to_csv(member_csv);

    EXPECT_EQ(read_text(direct_json), read_text(member_json));
    EXPECT_EQ(read_text(direct_csv), read_text(member_csv));
}

TEST(ExportTest, AutoTuneCsvExportWritesHeaderAndCandidateRows) {
    const std::filesystem::path path = make_output_path("autotune_result.csv");
    const autotune::AutoTuneResult result = make_result_fixture();

    export_csv(path, result);

    const std::vector<std::string> lines = read_lines(path);
    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines[0],
              "is_best,grid_wave_multiplier,grid_x,grid_y,grid_z,block_x,block_y,block_z,shared_mem,device_id,"
              "h2d_mean_ms,h2d_median_ms,h2d_p95_ms,h2d_ci95_low,h2d_ci95_high,h2d_cv,"
              "kernel_mean_ms,kernel_median_ms,kernel_p95_ms,kernel_ci95_low,kernel_ci95_high,kernel_cv,"
              "d2h_mean_ms,d2h_median_ms,d2h_p95_ms,d2h_ci95_low,d2h_ci95_high,d2h_cv,"
              "total_mean_ms,total_median_ms,total_p95_ms,total_ci95_low,total_ci95_high,total_cv");

    const std::vector<std::string> first_row = split_csv_row(lines[1]);
    const std::vector<std::string> second_row = split_csv_row(lines[2]);
    ASSERT_EQ(first_row.size(), 34U);
    ASSERT_EQ(second_row.size(), 34U);

    EXPECT_EQ(first_row[0], "false");
    EXPECT_EQ(first_row[1], "1");
    EXPECT_EQ(first_row[2], "4");
    EXPECT_EQ(first_row[5], "64");
    EXPECT_NEAR(std::stod(first_row[16]), 5.0, 1e-9);

    EXPECT_EQ(second_row[0], "true");
    EXPECT_EQ(second_row[1], "2");
    EXPECT_EQ(second_row[2], "2");
    EXPECT_EQ(second_row[5], "128");
    EXPECT_NEAR(std::stod(second_row[16]), 1.5, 1e-9);
}

TEST(ExportTest, AutoTuneCsvExportHandlesEmptyCandidateSet) {
    const std::filesystem::path path = make_output_path("autotune_empty.csv");
    const autotune::AutoTuneResult result;

    export_csv(path, result);

    const std::vector<std::string> lines = read_lines(path);
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_NE(lines[0].find("is_best,grid_wave_multiplier"), std::string::npos);
}

TEST(ExportTest, AutoTuneJsonExportWritesStructuredSummary) {
    const std::filesystem::path path = make_output_path("autotune_result.json");
    const autotune::AutoTuneResult result = make_result_fixture();

    export_json(path, result);

    const std::string text = read_text(path);
    EXPECT_NE(text.find("\"candidate_count\":2"), std::string::npos);
    EXPECT_NE(text.find("\"winner_index\":1"), std::string::npos);
    EXPECT_NE(text.find("\"reason\":\"Selected block=128, grid=2 because it had the lowest median_ms.\""),
              std::string::npos);
    EXPECT_NE(text.find("\"grid_wave_multiplier\":2"), std::string::npos);
    EXPECT_NE(text.find("\"is_best\":true"), std::string::npos);
    EXPECT_NE(text.find("\"median_ms\":1.5"), std::string::npos);
}

TEST(ExportTest, InvalidExportPathRaisesError) {
    const std::filesystem::path directory_path = make_output_path("directory_as_file");
    std::filesystem::create_directories(directory_path);

    EXPECT_THROW(export_csv(directory_path, make_benchmark_result_fixture()), std::runtime_error);
    EXPECT_THROW(export_json(directory_path, make_benchmark_result_fixture()), std::runtime_error);
    EXPECT_THROW(export_csv(directory_path, make_no_stage_report()), std::runtime_error);
    EXPECT_THROW(export_json(directory_path, make_no_stage_report()), std::runtime_error);
    EXPECT_THROW(export_csv(directory_path, make_result_fixture()), std::runtime_error);
    EXPECT_THROW(export_json(directory_path, make_result_fixture()), std::runtime_error);
}

} // namespace
} // namespace cuda_test::reporting

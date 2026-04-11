#include "cuda_test/pipeline/pipeline.hpp"
#include "cuda_test/pipeline/suite.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace cuda_test::pipeline {
namespace {

class ScopedTempDir {
public:
    explicit ScopedTempDir(std::string name)
        : path_(std::filesystem::temp_directory_path() / "cuda_test_pipeline_unit" /
                std::to_string(std::hash<std::string>{}(std::filesystem::current_path().string())) /
                std::move(name)) {
        std::filesystem::remove_all(path_);
    }

    ~ScopedTempDir() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream.is_open());

    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

[[nodiscard]] KernelDescriptor make_no_stage_descriptor(std::string name, std::size_t problem_size = 16U) {
    return describe_kernel(std::move(name))
        .problem_size(problem_size)
        .inputs([problem_size]() { return std::make_tuple(std::vector<int>(problem_size, 1)); })
        .expected([problem_size]() { return std::vector<int>(problem_size, 1); })
        .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
        .build();
}

TEST(PipelineSuiteTest, NoStagePipelineReturnsVacuousSuccess) {
    const PipelineReport report = make_pipeline(make_no_stage_descriptor("unit_alpha")).run();

    EXPECT_EQ(report.kernel_name(), "unit_alpha");
    EXPECT_EQ(report.device_id(), 0);
    EXPECT_FALSE(report.correctness_enabled());
    EXPECT_FALSE(report.correctness_passed());
    EXPECT_FALSE(report.benchmark_enabled());
    EXPECT_FALSE(report.autotune_enabled());
    EXPECT_FALSE(report.diagnose_enabled());
    EXPECT_TRUE(report.passed());
    EXPECT_FALSE(report.benchmark_result().has_value());
    EXPECT_FALSE(report.autotune_result().has_value());
    EXPECT_FALSE(report.fingerprint().has_value());
    EXPECT_TRUE(report.recommendations().empty());
}

TEST(PipelineSuiteTest, DeviceSelectionIsReflectedInNoStageReport) {
    const PipelineReport report = make_pipeline(make_no_stage_descriptor("unit_device")).device(3).run();

    EXPECT_EQ(report.device_id(), 3);
    EXPECT_TRUE(report.passed());
}

TEST(PipelineSuiteTest, DiagnoseWithoutTimingDataLeavesFingerprintEmpty) {
    const PipelineReport report = make_pipeline(make_no_stage_descriptor("unit_diagnose")).diagnose().run();

    EXPECT_TRUE(report.diagnose_enabled());
    EXPECT_FALSE(report.fingerprint().has_value());
    EXPECT_TRUE(report.recommendations().empty());
    EXPECT_TRUE(report.passed());
}

TEST(PipelineSuiteTest, PipelineReportExportsJsonAndCsvForNoStageRun) {
    ScopedTempDir temp_dir("cuda_test_pipeline_unit_report");
    const PipelineReport report = make_pipeline(make_no_stage_descriptor("unit_export")).device(5).run();

    const std::filesystem::path json_path = temp_dir.path() / "report.json";
    const std::filesystem::path csv_path = temp_dir.path() / "report.csv";

    report.to_json(json_path);
    report.to_csv(csv_path);

    ASSERT_TRUE(std::filesystem::exists(json_path));
    ASSERT_TRUE(std::filesystem::exists(csv_path));

    const std::string json = read_text_file(json_path);
    const std::string csv = read_text_file(csv_path);

    EXPECT_NE(json.find("\"kernel_name\":\"unit_export\""), std::string::npos);
    EXPECT_NE(json.find("\"device_id\":5"), std::string::npos);
    EXPECT_NE(json.find("\"passed\":true"), std::string::npos);
    EXPECT_NE(json.find("\"diagnose_enabled\":false"), std::string::npos);
    EXPECT_NE(json.find("\"correctness\":{\"enabled\":false,\"passed\":false}"), std::string::npos);
    EXPECT_EQ(json.find("\"benchmark\":"), std::string::npos);
    EXPECT_EQ(json.find("\"autotune\":"), std::string::npos);
    EXPECT_EQ(json.find("\"fingerprint\":"), std::string::npos);
    EXPECT_EQ(json.find("\"recommendations\":"), std::string::npos);
    EXPECT_NE(csv.find("kernel_name,device_id"), std::string::npos);
    EXPECT_NE(csv.find("\"unit_export\",5"), std::string::npos);
    EXPECT_NE(csv.find("diagnose_enabled"), std::string::npos);
}

TEST(PipelineSuiteTest, SuiteRejectsDuplicateAndExportCollidingNames) {
    Suite suite_runner("unit_suite");
    suite_runner.add(make_no_stage_descriptor("alpha beta"));

    EXPECT_THROW((void)suite_runner.add(make_no_stage_descriptor("alpha/beta")), std::invalid_argument);
}

TEST(PipelineSuiteTest, EmptyAutotuneBlockListThrows) {
    EXPECT_THROW((void)make_pipeline(make_no_stage_descriptor("neg_autotune"))
                         .autotune(std::vector<int>{})
                         .run(),
                 std::invalid_argument);
}

TEST(PipelineSuiteTest, InvalidBenchmarkConfigPropagatesException) {
    benchmark::BenchmarkConfig invalid_config;
    invalid_config.warmup_runs = -1;
    invalid_config.measure_runs = 0;

    EXPECT_THROW((void)make_pipeline(make_no_stage_descriptor("neg_benchmark"))
                         .benchmark(invalid_config)
                         .run(),
                 std::invalid_argument);
}

TEST(PipelineSuiteTest, EmptySuiteReturnsSuccessAndEmitsNoFiles) {
    ScopedTempDir json_dir("cuda_test_pipeline_unit_empty_json");
    ScopedTempDir csv_dir("cuda_test_pipeline_unit_empty_csv");

    const SuiteReport report = make_suite("empty_suite").run_all();

    EXPECT_TRUE(report.reports().empty());
    EXPECT_TRUE(report.all_passed());

    report.to_json(json_dir.path());
    report.to_csv(csv_dir.path());

    EXPECT_TRUE(std::filesystem::exists(json_dir.path()));
    EXPECT_TRUE(std::filesystem::exists(csv_dir.path()));
    EXPECT_TRUE(std::filesystem::is_empty(json_dir.path()));
    EXPECT_TRUE(std::filesystem::is_empty(csv_dir.path()));
}

TEST(PipelineSuiteTest, SuitePropagatesDiagnoseStageToEachReport) {
    const SuiteReport report = make_suite("diagnose_suite")
                                   .device(1)
                                   .diagnose()
                                   .add(make_no_stage_descriptor("diag_first"))
                                   .add(make_no_stage_descriptor("diag_second"))
                                   .run_all();

    ASSERT_EQ(report.reports().size(), 2U);
    for (const PipelineReport& item : report.reports()) {
        EXPECT_TRUE(item.diagnose_enabled());
        EXPECT_FALSE(item.fingerprint().has_value());
        EXPECT_TRUE(item.recommendations().empty());
        EXPECT_TRUE(item.passed());
    }
}

TEST(PipelineSuiteTest, SuiteWritesOneFilePerKernelForNoStageReports) {
    ScopedTempDir json_dir("cuda_test_pipeline_unit_suite_json");
    ScopedTempDir csv_dir("cuda_test_pipeline_unit_suite_csv");

    const SuiteReport report = make_suite("unit_suite")
                                   .device(2)
                                   .add(make_no_stage_descriptor("first_kernel"))
                                   .add(make_no_stage_descriptor("second_kernel"))
                                   .run_all();

    ASSERT_EQ(report.reports().size(), 2U);
    EXPECT_TRUE(report.all_passed());

    report.to_json(json_dir.path());
    report.to_csv(csv_dir.path());

    EXPECT_TRUE(std::filesystem::exists(json_dir.path() / "unit_suite_first_kernel.json"));
    EXPECT_TRUE(std::filesystem::exists(json_dir.path() / "unit_suite_second_kernel.json"));
    EXPECT_TRUE(std::filesystem::exists(csv_dir.path() / "unit_suite_first_kernel.csv"));
    EXPECT_TRUE(std::filesystem::exists(csv_dir.path() / "unit_suite_second_kernel.csv"));
}

} // namespace
} // namespace cuda_test::pipeline

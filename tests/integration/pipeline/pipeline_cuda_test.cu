#include "cuda_test/pipeline/pipeline.hpp"
#include "cuda_test/pipeline/suite.hpp"
#include "cuda_test/testing/kernel_test_fixture.hpp"
#include "tests/fixtures/vector_add_fixture.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cuda_test::pipeline {
namespace {

__global__ void vector_add_kernel(const float* lhs, const float* rhs, float* output, std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = lhs[index] + rhs[index];
    }
}

class ScopedTempDir {
public:
    explicit ScopedTempDir(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name)) {
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

class PipelineCudaTest : public testing::KernelTestFixture {
protected:
    void SetUp() override {
        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }

        testing::KernelTestFixture::SetUp();
    }

    [[nodiscard]] KernelDescriptor make_vector_add_descriptor(
        std::string name,
        std::size_t size,
        bool wrong_expected = false,
        std::optional<unsigned int> expected_block_x = std::nullopt,
        std::optional<unsigned int> expected_grid_x = std::nullopt,
        std::optional<int> expected_device_id = std::nullopt) const {
        const auto fixture = tests::fixtures::make_vector_add_fixture(size);
        std::vector<float> expected = fixture.expected;
        if (wrong_expected && !expected.empty()) {
            expected.front() += 1.0f;
        }

        return describe_kernel(std::move(name))
            .problem_size(size)
            .inputs([lhs = fixture.lhs, rhs = fixture.rhs]() { return std::make_tuple(lhs, rhs); })
            .expected([expected = std::move(expected)]() { return expected; })
            .tolerance(1e-5)
            .launch([size, expected_block_x, expected_grid_x, expected_device_id](
                        const core::KernelLaunchConfig& config,
                        auto& inputs,
                        auto& output) {
                if (expected_block_x.has_value() && config.block.x != *expected_block_x) {
                    throw std::runtime_error("Unexpected block.x");
                }
                if (expected_grid_x.has_value() && config.grid.x != *expected_grid_x) {
                    throw std::runtime_error("Unexpected grid.x");
                }
                if (expected_device_id.has_value() && config.device_id != *expected_device_id) {
                    throw std::runtime_error("Unexpected device_id");
                }

                auto& [lhs, rhs] = inputs;
                vector_add_kernel<<<config.grid, config.block, config.shared_mem>>>(
                    lhs.data(), rhs.data(), output.data(), size);
            })
            .build();
    }

    [[nodiscard]] autotune::AutoTuneSpec make_fast_autotune_spec(std::vector<int> block_sizes,
                                                                 std::vector<int> multipliers) const {
        autotune::AutoTuneSpec spec;
        spec.block_sizes = std::move(block_sizes);
        spec.grid_wave_multipliers = std::move(multipliers);
        spec.warmup_runs = 1;
        spec.measure_runs = 3;
        return spec;
    }

    [[nodiscard]] benchmark::BenchmarkConfig make_fast_benchmark_config(int warmup_runs,
                                                                        int measure_runs) const {
        benchmark::BenchmarkConfig config;
        config.warmup_runs = warmup_runs;
        config.measure_runs = measure_runs;
        return config;
    }
};

TEST_F(PipelineCudaTest, CorrectnessOnlyReportPassesForValidDescriptor) {
    const KernelDescriptor descriptor = make_vector_add_descriptor("correctness_only", 1024);

    const PipelineReport report =
        make_pipeline(descriptor).device(selected_device_id()).correctness().run();

    EXPECT_TRUE(report.correctness_enabled());
    EXPECT_TRUE(report.correctness_passed());
    EXPECT_TRUE(report.passed());
    EXPECT_FALSE(report.benchmark_result().has_value());
    EXPECT_FALSE(report.autotune_result().has_value());
}

TEST_F(PipelineCudaTest, FailingCorrectnessSkipsBenchmarkAndAutotune) {
    const KernelDescriptor descriptor = make_vector_add_descriptor("correctness_fail", 1024, true);
    const autotune::AutoTuneSpec spec = make_fast_autotune_spec({64, 128}, {1, 2});
    const benchmark::BenchmarkConfig benchmark_config = make_fast_benchmark_config(1, 2);

    const PipelineReport report = make_pipeline(descriptor)
                                      .device(selected_device_id())
                                      .correctness()
                                      .benchmark(benchmark_config)
                                      .autotune(spec)
                                      .run();

    EXPECT_TRUE(report.correctness_enabled());
    EXPECT_FALSE(report.correctness_passed());
    EXPECT_FALSE(report.passed());
    EXPECT_FALSE(report.benchmark_result().has_value());
    EXPECT_FALSE(report.autotune_result().has_value());
}

TEST_F(PipelineCudaTest, BenchmarkOnlyUsesBaselineConfigurationAndProducesStats) {
    const KernelDescriptor descriptor = make_vector_add_descriptor(
        "benchmark_only", 1000, false, 128U, 8U, selected_device_id());
    const benchmark::BenchmarkConfig benchmark_config = make_fast_benchmark_config(1, 3);

    const PipelineReport report =
        make_pipeline(descriptor).device(selected_device_id()).benchmark(benchmark_config).run();

    ASSERT_TRUE(report.benchmark_result().has_value());
    EXPECT_FALSE(report.correctness_enabled());
    EXPECT_TRUE(report.passed());
    EXPECT_GT(report.benchmark_result()->kernel_stats.mean_ms, 0.0);
    EXPECT_GT(report.benchmark_result()->kernel_stats.median_ms, 0.0);
    EXPECT_GT(report.benchmark_result()->samples.size(), 0U);
}

TEST_F(PipelineCudaTest, AutotuneOnlyReturnsAllCandidatesAndPropagatesDevice) {
    const KernelDescriptor descriptor = make_vector_add_descriptor("autotune_only", 4096);
    const autotune::AutoTuneSpec spec = make_fast_autotune_spec({64, 128}, {1, 2});

    const PipelineReport report =
        make_pipeline(descriptor).device(selected_device_id()).autotune(spec).run();

    ASSERT_TRUE(report.autotune_result().has_value());
    EXPECT_TRUE(report.passed());
    EXPECT_EQ(report.device_id(), selected_device_id());
    EXPECT_EQ(report.autotune_result()->all_candidates.size(), 4U);
    EXPECT_EQ(report.autotune_result()->best.device_id, selected_device_id());

    for (const autotune::CandidateRecord& candidate : report.autotune_result()->all_candidates) {
        EXPECT_EQ(candidate.config.device_id, selected_device_id());
    }
}

TEST_F(PipelineCudaTest, FullChainPopulatesAllStagesAndExportsReport) {
    ScopedTempDir temp_dir("cuda_test_pipeline_cuda_full_chain");
    const KernelDescriptor descriptor = make_vector_add_descriptor("full_chain", 65536);
    const autotune::AutoTuneSpec spec = make_fast_autotune_spec({64, 128}, {1});
    const benchmark::BenchmarkConfig benchmark_config = make_fast_benchmark_config(1, 3);

    const PipelineReport report = make_pipeline(descriptor)
                                      .device(selected_device_id())
                                      .correctness()
                                      .benchmark(benchmark_config)
                                      .autotune(spec)
                                      .run();

    ASSERT_TRUE(report.correctness_enabled());
    ASSERT_TRUE(report.correctness_passed());
    ASSERT_TRUE(report.benchmark_result().has_value());
    ASSERT_TRUE(report.autotune_result().has_value());
    EXPECT_TRUE(report.passed());

    const std::filesystem::path json_path = temp_dir.path() / "full_chain.json";
    const std::filesystem::path csv_path = temp_dir.path() / "full_chain.csv";
    report.to_json(json_path);
    report.to_csv(csv_path);

    ASSERT_TRUE(std::filesystem::exists(json_path));
    ASSERT_TRUE(std::filesystem::exists(csv_path));

    const std::string json = read_text_file(json_path);
    const std::string csv = read_text_file(csv_path);
    EXPECT_NE(json.find("\"kernel_name\":\"full_chain\""), std::string::npos);
    EXPECT_NE(json.find("\"autotune_result\":"), std::string::npos);
    EXPECT_NE(csv.find("autotune_candidate_count"), std::string::npos);
    EXPECT_NE(csv.find("\"full_chain\""), std::string::npos);
}

TEST_F(PipelineCudaTest, SuiteAppliesConfiguredStagesToAllDescriptors) {
    const benchmark::BenchmarkConfig benchmark_config = make_fast_benchmark_config(1, 2);

    const SuiteReport report =
        make_suite("suite_success")
            .device(selected_device_id())
            .correctness()
            .benchmark(benchmark_config)
            .add(make_vector_add_descriptor("suite_first", 2048))
            .add(make_vector_add_descriptor("suite_second", 2048))
            .run_all();

    ASSERT_EQ(report.reports().size(), 2U);
    EXPECT_TRUE(report.all_passed());

    for (const PipelineReport& item : report.reports()) {
        EXPECT_TRUE(item.correctness_enabled());
        EXPECT_TRUE(item.correctness_passed());
        EXPECT_TRUE(item.benchmark_enabled());
        EXPECT_TRUE(item.benchmark_result().has_value());
    }
}

TEST_F(PipelineCudaTest, SuiteReportsPartialFailureAndWritesFiles) {
    ScopedTempDir json_dir("cuda_test_pipeline_cuda_suite_json");
    ScopedTempDir csv_dir("cuda_test_pipeline_cuda_suite_csv");
    const benchmark::BenchmarkConfig benchmark_config = make_fast_benchmark_config(1, 2);

    const SuiteReport report =
        make_suite("suite_mixed")
            .device(selected_device_id())
            .correctness()
            .benchmark(benchmark_config)
            .add(make_vector_add_descriptor("suite_ok", 2048))
            .add(make_vector_add_descriptor("suite_bad", 2048, true))
            .run_all();

    ASSERT_EQ(report.reports().size(), 2U);
    EXPECT_FALSE(report.all_passed());

    const auto ok_report = std::find_if(report.reports().begin(), report.reports().end(), [](const auto& item) {
        return item.kernel_name() == "suite_ok";
    });
    const auto bad_report =
        std::find_if(report.reports().begin(), report.reports().end(), [](const auto& item) {
            return item.kernel_name() == "suite_bad";
        });

    ASSERT_NE(ok_report, report.reports().end());
    ASSERT_NE(bad_report, report.reports().end());
    EXPECT_TRUE(ok_report->passed());
    EXPECT_TRUE(ok_report->benchmark_result().has_value());
    EXPECT_FALSE(bad_report->passed());
    EXPECT_FALSE(bad_report->benchmark_result().has_value());
    EXPECT_FALSE(bad_report->autotune_result().has_value());

    report.to_json(json_dir.path());
    report.to_csv(csv_dir.path());

    EXPECT_TRUE(std::filesystem::exists(json_dir.path() / "suite_mixed_suite_ok.json"));
    EXPECT_TRUE(std::filesystem::exists(json_dir.path() / "suite_mixed_suite_bad.json"));
    EXPECT_TRUE(std::filesystem::exists(csv_dir.path() / "suite_mixed_suite_ok.csv"));
    EXPECT_TRUE(std::filesystem::exists(csv_dir.path() / "suite_mixed_suite_bad.csv"));
}

} // namespace
} // namespace cuda_test::pipeline

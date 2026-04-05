#include "cuda_test/pipeline/pipeline.hpp"
#include "cuda_test/testing/kernel_test_fixture.hpp"
#include "tests/fixtures/vector_add_fixture.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
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

class PipelineDiagnoseCudaTest : public testing::KernelTestFixture {
protected:
    void SetUp() override {
        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }

        testing::KernelTestFixture::SetUp();
    }

    [[nodiscard]] KernelDescriptor make_vector_add_descriptor(std::string name, std::size_t size) const {
        const auto fixture = tests::fixtures::make_vector_add_fixture(size);

        return describe_kernel(std::move(name))
            .problem_size(size)
            .inputs([lhs = fixture.lhs, rhs = fixture.rhs]() { return std::make_tuple(lhs, rhs); })
            .expected([expected = fixture.expected]() { return expected; })
            .tolerance(1e-5)
            .launch([size](const core::KernelLaunchConfig& config, auto& inputs, auto& output) {
                auto& [lhs, rhs] = inputs;
                vector_add_kernel<<<config.grid, config.block, config.shared_mem>>>(
                    lhs.data(), rhs.data(), output.data(), size);
            })
            .build();
    }

    [[nodiscard]] benchmark::BenchmarkConfig make_fast_benchmark_config() const {
        benchmark::BenchmarkConfig config;
        config.warmup_runs = 1;
        config.measure_runs = 3;
        return config;
    }

    [[nodiscard]] autotune::AutoTuneSpec make_fast_autotune_spec() const {
        autotune::AutoTuneSpec spec;
        spec.block_sizes = {64, 128, 256};
        spec.grid_wave_multipliers = {1};
        spec.warmup_runs = 1;
        spec.measure_runs = 3;
        return spec;
    }
};

TEST_F(PipelineDiagnoseCudaTest, BenchmarkOnlyDiagnosePopulatesFingerprint) {
    const PipelineReport report =
        make_pipeline(make_vector_add_descriptor("diagnose_benchmark_only", 4096))
            .device(selected_device_id())
            .benchmark(make_fast_benchmark_config())
            .diagnose()
            .run();

    ASSERT_TRUE(report.diagnose_enabled());
    ASSERT_TRUE(report.fingerprint().has_value());
    EXPECT_GE(report.fingerprint()->transfer_compute_ratio, 0.0);
    EXPECT_GE(report.fingerprint()->cv, 0.0);
    EXPECT_DOUBLE_EQ(report.fingerprint()->block_sensitivity, 0.0);
}

TEST_F(PipelineDiagnoseCudaTest, AutotuneOnlyDiagnoseUsesWinningCandidateAndComputesBlockSensitivity) {
    const PipelineReport report =
        make_pipeline(make_vector_add_descriptor("diagnose_autotune_only", 8192))
            .device(selected_device_id())
            .autotune(make_fast_autotune_spec())
            .diagnose()
            .run();

    ASSERT_TRUE(report.diagnose_enabled());
    ASSERT_TRUE(report.autotune_result().has_value());
    ASSERT_TRUE(report.fingerprint().has_value());
    EXPECT_GE(report.fingerprint()->transfer_compute_ratio, 0.0);
    EXPECT_GT(report.fingerprint()->block_sensitivity, 0.0);
    EXPECT_GE(report.recommendations().size(), 0U);
}

TEST_F(PipelineDiagnoseCudaTest, DiagnoseExportIncludesFingerprintAndRecommendationsEnvelope) {
    ScopedTempDir temp_dir("cuda_test_pipeline_diagnose_cuda");

    const PipelineReport report =
        make_pipeline(make_vector_add_descriptor("diagnose_export", 2048))
            .device(selected_device_id())
            .benchmark(make_fast_benchmark_config())
            .autotune(make_fast_autotune_spec())
            .diagnose()
            .run();

    const std::filesystem::path json_path = temp_dir.path() / "diagnose.json";
    const std::filesystem::path csv_path = temp_dir.path() / "diagnose.csv";
    report.to_json(json_path);
    report.to_csv(csv_path);

    const std::string json = read_text_file(json_path);
    const std::string csv = read_text_file(csv_path);

    EXPECT_NE(json.find("\"diagnose_enabled\":true"), std::string::npos);
    EXPECT_NE(json.find("\"fingerprint\":"), std::string::npos);
    EXPECT_NE(json.find("\"recommendations\":"), std::string::npos);

    EXPECT_NE(csv.find("diagnose_enabled"), std::string::npos);
    EXPECT_NE(csv.find("fingerprint_transfer_compute_ratio"), std::string::npos);
    EXPECT_NE(csv.find("recommendation_count"), std::string::npos);
    EXPECT_NE(csv.find("\"diagnose_export\""), std::string::npos);
}

} // namespace
} // namespace cuda_test::pipeline

#include "cuda_test/pipeline/kernel_descriptor.hpp"
#include "cuda_test/testing/kernel_test_fixture.hpp"
#include "tests/fixtures/vector_add_fixture.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
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

__global__ void int_identity_kernel(const int* input, int* output, std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = input[index];
    }
}

class KernelDescriptorCudaTest : public testing::KernelTestFixture {
protected:
    void SetUp() override {
        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }

        testing::KernelTestFixture::SetUp();
    }

    [[nodiscard]] auto make_vector_add_descriptor(std::size_t size) const {
        const auto fixture = tests::fixtures::make_vector_add_fixture(size);
        return describe_kernel("vector_add")
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

    [[nodiscard]] auto make_wrong_expected_descriptor(std::size_t size) const {
        const auto fixture = tests::fixtures::make_vector_add_fixture(size);
        std::vector<float> wrong_expected = fixture.expected;
        if (!wrong_expected.empty()) {
            wrong_expected.front() += 0.5f;
        }

        return describe_kernel("vector_add_wrong")
            .problem_size(size)
            .inputs([lhs = fixture.lhs, rhs = fixture.rhs]() { return std::make_tuple(lhs, rhs); })
            .expected([expected = std::move(wrong_expected)]() { return expected; })
            .tolerance(1e-5)
            .launch([size](const core::KernelLaunchConfig& config, auto& inputs, auto& output) {
                auto& [lhs, rhs] = inputs;
                vector_add_kernel<<<config.grid, config.block, config.shared_mem>>>(
                    lhs.data(), rhs.data(), output.data(), size);
            })
            .build();
    }

    [[nodiscard]] auto make_identity_descriptor(std::size_t size, bool wrong_expected) const {
        std::vector<int> input(size);
        std::vector<int> expected(size);
        for (std::size_t index = 0; index < size; ++index) {
            input[index] = static_cast<int>(index);
            expected[index] = static_cast<int>(wrong_expected ? index + 1U : index);
        }

        return describe_kernel("identity_int")
            .problem_size(size)
            .inputs([input]() { return std::make_tuple(input); })
            .expected([expected]() { return expected; })
            .launch([size](const core::KernelLaunchConfig& config, auto& inputs, auto& output) {
                auto& [input] = inputs;
                int_identity_kernel<<<config.grid, config.block, config.shared_mem>>>(
                    input.data(), output.data(), size);
            })
            .build();
    }
};

TEST_F(KernelDescriptorCudaTest, ValidateReturnsTrueForMatchingFloatKernel) {
    const auto descriptor = make_vector_add_descriptor(1024);
    const core::KernelLaunchConfig config = descriptor.baseline_config(selected_device_id());

    EXPECT_TRUE(descriptor.validate(config));
}

TEST_F(KernelDescriptorCudaTest, ValidateReturnsFalseForWrongExpectedFloatKernel) {
    const auto descriptor = make_wrong_expected_descriptor(1024);
    const core::KernelLaunchConfig config = descriptor.baseline_config(selected_device_id());

    EXPECT_FALSE(descriptor.validate(config));
}

TEST_F(KernelDescriptorCudaTest, ValidateUsesExactMatchForIntegralOutputs) {
    const auto correct_descriptor = make_identity_descriptor(1024, false);
    const auto wrong_descriptor = make_identity_descriptor(1024, true);
    const core::KernelLaunchConfig config = correct_descriptor.baseline_config(selected_device_id());

    EXPECT_TRUE(correct_descriptor.validate(config));
    EXPECT_FALSE(wrong_descriptor.validate(config));
}

TEST_F(KernelDescriptorCudaTest, MeasureReturnsPositiveBreakdownForNonTrivialInput) {
    const auto descriptor = make_vector_add_descriptor(65536);
    const core::KernelLaunchConfig config = descriptor.baseline_config(selected_device_id());

    const core::ProfilingBreakdown breakdown = descriptor.measure(config);
    EXPECT_GT(breakdown.h2d_ms, 0.0);
    EXPECT_GT(breakdown.kernel_ms, 0.0);
    EXPECT_GT(breakdown.d2h_ms, 0.0);
    EXPECT_GT(breakdown.total_ms, 0.0);
}

TEST_F(KernelDescriptorCudaTest, MeasureKeepsTotalStageGreaterThanComponentStages) {
    const auto descriptor = make_vector_add_descriptor(65536);
    const core::KernelLaunchConfig config = descriptor.baseline_config(selected_device_id());

    const core::ProfilingBreakdown breakdown = descriptor.measure(config);
    EXPECT_GE(breakdown.total_ms, breakdown.h2d_ms);
    EXPECT_GE(breakdown.total_ms, breakdown.kernel_ms);
    EXPECT_GE(breakdown.total_ms, breakdown.d2h_ms);
}

TEST_F(KernelDescriptorCudaTest, MeasureCanBeCalledRepeatedlyOnTheSameDescriptor) {
    const auto descriptor = make_vector_add_descriptor(65536);
    const core::KernelLaunchConfig config = descriptor.baseline_config(selected_device_id());

    const core::ProfilingBreakdown first = descriptor.measure(config);
    const core::ProfilingBreakdown second = descriptor.measure(config);

    EXPECT_GT(first.total_ms, 0.0);
    EXPECT_GT(second.total_ms, 0.0);
}

TEST_F(KernelDescriptorCudaTest, MovedDescriptorRetainsValidateAndMeasureBehavior) {
    auto original = make_vector_add_descriptor(4096);
    KernelDescriptor moved(std::move(original));
    const core::KernelLaunchConfig config = moved.baseline_config(selected_device_id());

    EXPECT_TRUE(moved.validate(config));

    const core::ProfilingBreakdown breakdown = moved.measure(config);
    EXPECT_GT(breakdown.total_ms, 0.0);
}

TEST_F(KernelDescriptorCudaTest, MeasurePropagatesInvalidLaunchConfiguration) {
    const auto descriptor = make_vector_add_descriptor(1024);
    core::KernelLaunchConfig invalid = descriptor.baseline_config(selected_device_id());
    invalid.block = dim3(0U, 1U, 1U);
    invalid.grid = dim3(1U, 1U, 1U);

    EXPECT_THROW((void)descriptor.measure(invalid), std::runtime_error);
}

TEST_F(KernelDescriptorCudaTest, ValidateReturnsTrueForZeroProblemSize) {
    const auto descriptor = describe_kernel("zero_size")
                                .problem_size(0)
                                .inputs([]() { return std::make_tuple(std::vector<int>{}); })
                                .expected([]() { return std::vector<int>{}; })
                                .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
                                .build();

    EXPECT_TRUE(descriptor.validate(descriptor.baseline_config(selected_device_id())));
    const core::ProfilingBreakdown breakdown = descriptor.measure(
        descriptor.baseline_config(selected_device_id()));
    EXPECT_EQ(breakdown.h2d_ms, 0.0);
    EXPECT_EQ(breakdown.kernel_ms, 0.0);
    EXPECT_EQ(breakdown.d2h_ms, 0.0);
    EXPECT_EQ(breakdown.total_ms, 0.0);
}

} // namespace
} // namespace cuda_test::pipeline

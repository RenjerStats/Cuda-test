#include "cuda_test/pipeline/kernel_descriptor.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace cuda_test::pipeline {
namespace {

TEST(KernelDescriptorBuilderTest, BuildsDescriptorAndExposesMetadata) {
    auto descriptor = describe_kernel("host_mock")
                          .problem_size(1024)
                          .inputs([]() {
                              return std::make_tuple(std::vector<int>(1024, 1), std::vector<int>(1024, 2));
                          })
                          .expected([]() { return std::vector<int>(1024, 3); })
                          .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
                          .build();

    EXPECT_EQ(descriptor.name(), "host_mock");
    EXPECT_EQ(descriptor.problem_size(), 1024U);
}

TEST(KernelDescriptorBuilderTest, BuildThrowsWhenRequiredFieldsAreMissing) {
    EXPECT_THROW((void)describe_kernel("missing_everything").build(), std::invalid_argument);

    auto missing_launch = describe_kernel("missing_launch")
                              .problem_size(16)
                              .inputs([]() { return std::make_tuple(std::vector<int>(16, 1)); })
                              .expected([]() { return std::vector<int>(16, 1); });
    EXPECT_THROW((void)missing_launch.build(), std::invalid_argument);

    auto missing_inputs = describe_kernel("missing_inputs")
                              .problem_size(16)
                              .expected([]() { return std::vector<int>(16, 1); })
                              .launch([](const core::KernelLaunchConfig&, auto&, auto&) {});
    EXPECT_THROW((void)missing_inputs.build(), std::invalid_argument);
}

TEST(KernelDescriptorBuilderTest, BaselineConfigUsesDefaultBlockAndCoversProblem) {
    const auto descriptor = describe_kernel("baseline")
                                .problem_size(1024)
                                .inputs([]() { return std::make_tuple(std::vector<int>(1024, 7)); })
                                .expected([]() { return std::vector<int>(1024, 7); })
                                .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
                                .build();

    const core::KernelLaunchConfig config = descriptor.baseline_config();
    EXPECT_EQ(config.block.x, 128U);
    EXPECT_EQ(config.grid.x, 8U);
    EXPECT_EQ(config.grid.y, 1U);
    EXPECT_EQ(config.block.y, 1U);
    EXPECT_EQ(config.shared_mem, 0U);
}

TEST(KernelDescriptorBuilderTest, BaselineConfigRoundsUpForNonAlignedProblemSizes) {
    const auto descriptor = describe_kernel("baseline_non_aligned")
                                .problem_size(1000)
                                .inputs([]() { return std::make_tuple(std::vector<int>(1000, 1)); })
                                .expected([]() { return std::vector<int>(1000, 1); })
                                .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
                                .build();

    const core::KernelLaunchConfig config = descriptor.baseline_config();
    EXPECT_EQ(config.block.x, 128U);
    EXPECT_EQ(config.grid.x, 8U);
    EXPECT_GE(static_cast<std::size_t>(config.grid.x) * static_cast<std::size_t>(config.block.x), 1000U);
}

TEST(KernelDescriptorBuilderTest, MoveConstructionPreservesMetadata) {
    auto original = describe_kernel("movable")
                        .problem_size(64)
                        .inputs([]() { return std::make_tuple(std::vector<int>(64, 1)); })
                        .expected([]() { return std::vector<int>(64, 1); })
                        .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
                        .build();

    KernelDescriptor moved(std::move(original));
    EXPECT_EQ(moved.name(), "movable");
    EXPECT_EQ(moved.problem_size(), 64U);

    const core::KernelLaunchConfig config = moved.baseline_config();
    EXPECT_EQ(config.block.x, 128U);
    EXPECT_EQ(config.grid.x, 1U);
}

TEST(KernelDescriptorBuilderTest, BaselineConfigRejectsUnavailableDeviceWhenRuntimeSeesGPUs) {
    if (core::device_count() <= 0) {
        GTEST_SKIP() << "No CUDA device is visible to the runtime";
    }

    const auto descriptor = describe_kernel("device_check")
                                .problem_size(32)
                                .inputs([]() { return std::make_tuple(std::vector<int>(32, 1)); })
                                .expected([]() { return std::vector<int>(32, 1); })
                                .launch([](const core::KernelLaunchConfig&, auto&, auto&) {})
                                .build();

    EXPECT_THROW((void)descriptor.baseline_config(core::device_count()), std::out_of_range);
}

} // namespace
} // namespace cuda_test::pipeline

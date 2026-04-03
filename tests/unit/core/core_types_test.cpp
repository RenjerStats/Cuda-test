#include "cuda_test/core/types.hpp"

#include <gtest/gtest.h>

namespace cuda_test::core {
namespace {

TEST(KernelLaunchConfigTest, DefaultsMatchPlan) {
    const KernelLaunchConfig config{};

    EXPECT_EQ(config.grid.x, 1U);
    EXPECT_EQ(config.grid.y, 1U);
    EXPECT_EQ(config.grid.z, 1U);
    EXPECT_EQ(config.block.x, 1U);
    EXPECT_EQ(config.block.y, 1U);
    EXPECT_EQ(config.block.z, 1U);
    EXPECT_EQ(config.shared_mem, 0U);
    EXPECT_EQ(config.device_id, 0);
}

TEST(ProfilingBreakdownTest, DefaultsToZero) {
    const ProfilingBreakdown breakdown{};

    EXPECT_DOUBLE_EQ(breakdown.h2d_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.kernel_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.d2h_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.total_ms, 0.0);
}

TEST(RunStatsTest, DefaultsToZero) {
    const RunStats stats{};

    EXPECT_DOUBLE_EQ(stats.mean_ms, 0.0);
    EXPECT_DOUBLE_EQ(stats.median_ms, 0.0);
    EXPECT_DOUBLE_EQ(stats.p95_ms, 0.0);
    EXPECT_DOUBLE_EQ(stats.ci95_low, 0.0);
    EXPECT_DOUBLE_EQ(stats.ci95_high, 0.0);
    EXPECT_DOUBLE_EQ(stats.cv, 0.0);
}

} // namespace
} // namespace cuda_test::core

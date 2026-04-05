#include "cuda_test/analysis/bandwidth.hpp"
#include "cuda_test/core/device_info.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace cuda_test::analysis {
namespace {

TEST(BandwidthTest, ComputesExpectedAchievedBandwidth) {
#if !defined(CUDA_TEST_HAS_CUDA) || !CUDA_TEST_HAS_CUDA
    GTEST_SKIP() << "CUDA support is disabled";
#else
    if (core::device_count() <= 0) {
        GTEST_SKIP() << "No CUDA device is visible to the runtime";
    }

    const BandwidthInfo info = estimate_bandwidth(1'000'000'000ULL, 10.0, 0);
    EXPECT_NEAR(info.achieved_gbps, 100.0, 1e-9);
    EXPECT_GT(info.theoretical_gbps, 0.0);
    EXPECT_GE(info.utilization_ratio, 0.0);
    EXPECT_LE(info.utilization_ratio, 1.0);
#endif
}

TEST(BandwidthTest, ZeroTransferProducesZeroAchievedBandwidth) {
#if !defined(CUDA_TEST_HAS_CUDA) || !CUDA_TEST_HAS_CUDA
    GTEST_SKIP() << "CUDA support is disabled";
#else
    if (core::device_count() <= 0) {
        GTEST_SKIP() << "No CUDA device is visible to the runtime";
    }

    const BandwidthInfo info = estimate_bandwidth(0, 1.0, 0);
    EXPECT_DOUBLE_EQ(info.achieved_gbps, 0.0);
    EXPECT_DOUBLE_EQ(info.utilization_ratio, 0.0);
    EXPECT_GT(info.theoretical_gbps, 0.0);
#endif
}

TEST(BandwidthTest, RejectsNonPositiveKernelTime) {
    EXPECT_THROW((void)estimate_bandwidth(1024, 0.0, 0), std::invalid_argument);
    EXPECT_THROW((void)estimate_bandwidth(1024, -1.0, 0), std::invalid_argument);
}

} // namespace
} // namespace cuda_test::analysis

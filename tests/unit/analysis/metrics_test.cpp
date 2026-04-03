#include "cuda_test/analysis/metrics.hpp"

#include <gtest/gtest.h>

namespace cuda_test::analysis {
namespace {

TEST(TransferComputeRatioTest, ComputesExpectedRatio) {
    const core::ProfilingBreakdown breakdown{1.0, 2.0, 1.0, 4.0};

    EXPECT_DOUBLE_EQ(transfer_compute_ratio(breakdown), 1.0);
}

TEST(TransferComputeRatioTest, RejectsNonPositiveKernelTime) {
    EXPECT_THROW((void)transfer_compute_ratio(core::ProfilingBreakdown{1.0, 0.0, 1.0, 2.0}),
                 std::invalid_argument);
    EXPECT_THROW((void)transfer_compute_ratio(core::ProfilingBreakdown{1.0, -1.0, 1.0, 1.0}),
                 std::invalid_argument);
}

} // namespace
} // namespace cuda_test::analysis

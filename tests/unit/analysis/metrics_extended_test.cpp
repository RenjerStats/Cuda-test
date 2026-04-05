#include "cuda_test/analysis/fingerprint.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace cuda_test::analysis {
namespace {

autotune::CandidateRecord make_candidate(double median_ms) {
    autotune::CandidateRecord candidate;
    candidate.benchmark.kernel_stats.median_ms = median_ms;
    return candidate;
}

TEST(BlockSensitivityTest, ReturnsOneForIdenticalCandidates) {
    const std::vector<autotune::CandidateRecord> candidates{
        make_candidate(5.0),
        make_candidate(5.0),
        make_candidate(5.0),
    };

    EXPECT_DOUBLE_EQ(block_sensitivity(candidates), 1.0);
}

TEST(BlockSensitivityTest, ReturnsRatioForDiverseCandidates) {
    const std::vector<autotune::CandidateRecord> candidates{
        make_candidate(2.0),
        make_candidate(4.0),
        make_candidate(6.0),
    };

    EXPECT_DOUBLE_EQ(block_sensitivity(candidates), 3.0);
}

TEST(BlockSensitivityTest, ReturnsOneForSingleCandidate) {
    const std::vector<autotune::CandidateRecord> candidates{make_candidate(3.0)};

    EXPECT_DOUBLE_EQ(block_sensitivity(candidates), 1.0);
}

TEST(BlockSensitivityTest, RejectsEmptyCandidateSet) {
    EXPECT_THROW((void)block_sensitivity({}), std::invalid_argument);
}

TEST(ScalingExponentTest, ReturnsOneForLinearData) {
    const std::vector<std::pair<std::size_t, double>> samples{
        {100U, 1.0},
        {1000U, 10.0},
        {10000U, 100.0},
    };

    EXPECT_NEAR(scaling_exponent(samples), 1.0, 0.05);
}

TEST(ScalingExponentTest, ReturnsTwoForQuadraticData) {
    const std::vector<std::pair<std::size_t, double>> samples{
        {10U, 100.0},
        {100U, 10000.0},
        {1000U, 1000000.0},
    };

    EXPECT_NEAR(scaling_exponent(samples), 2.0, 0.05);
}

TEST(ScalingExponentTest, ReturnsZeroForConstantData) {
    const std::vector<std::pair<std::size_t, double>> samples{
        {100U, 5.0},
        {1000U, 5.0},
        {10000U, 5.0},
    };

    EXPECT_NEAR(scaling_exponent(samples), 0.0, 0.05);
}

TEST(ScalingExponentTest, RejectsTooFewSamples) {
    EXPECT_THROW((void)scaling_exponent({}), std::invalid_argument);
    EXPECT_THROW((void)scaling_exponent({{100U, 1.0}}), std::invalid_argument);
}

TEST(ScalingExponentTest, RejectsZeroProblemSize) {
    EXPECT_THROW((void)scaling_exponent({{0U, 1.0}, {100U, 2.0}}), std::invalid_argument);
}

TEST(ScalingExponentTest, RejectsNegativeTime) {
    EXPECT_THROW((void)scaling_exponent({{100U, -1.0}, {200U, 2.0}}), std::invalid_argument);
}

TEST(KernelFingerprintTest, BuildsFromAvailableMetrics) {
    core::ProfilingBreakdown breakdown;
    breakdown.h2d_ms = 2.0;
    breakdown.kernel_ms = 4.0;
    breakdown.d2h_ms = 1.0;

    core::RunStats kernel_stats;
    kernel_stats.cv = 0.1;

    OccupancyInfo occupancy;
    occupancy.occupancy_ratio = 0.75;

    BandwidthInfo bandwidth;
    bandwidth.utilization_ratio = 0.5;

    KernelAttributes attributes;
    attributes.num_regs = 32;
    attributes.local_size_bytes = 16U;
    attributes.shared_size_bytes = 64U;

    const KernelFingerprint fingerprint =
        build_fingerprint(breakdown, kernel_stats, occupancy, bandwidth, attributes, 1.4, 1.1);

    EXPECT_DOUBLE_EQ(fingerprint.transfer_compute_ratio, 0.75);
    EXPECT_DOUBLE_EQ(fingerprint.occupancy, 0.75);
    EXPECT_DOUBLE_EQ(fingerprint.bandwidth_utilization, 0.5);
    EXPECT_DOUBLE_EQ(fingerprint.cv, 0.1);
    EXPECT_DOUBLE_EQ(fingerprint.block_sensitivity, 1.4);
    EXPECT_DOUBLE_EQ(fingerprint.scaling_exponent, 1.1);
    EXPECT_EQ(fingerprint.num_regs, 32);
    EXPECT_EQ(fingerprint.local_size_bytes, 16U);
    EXPECT_EQ(fingerprint.shared_size_bytes, 64U);
}

} // namespace
} // namespace cuda_test::analysis

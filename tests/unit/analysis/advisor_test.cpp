#include "cuda_test/analysis/advisor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace cuda_test::analysis {
namespace {

KernelFingerprint make_healthy_fingerprint() {
    KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = 0.5;
    fingerprint.occupancy = 0.8;
    fingerprint.has_occupancy = true;
    fingerprint.bandwidth_utilization = 0.3;
    fingerprint.has_bandwidth_utilization = true;
    fingerprint.cv = 0.05;
    fingerprint.block_sensitivity = 1.1;
    fingerprint.scaling_exponent = 1.0;
    fingerprint.has_scaling_exponent = true;
    fingerprint.num_regs = 16;
    fingerprint.has_kernel_attributes = true;
    return fingerprint;
}

bool contains_tag(const std::vector<Recommendation>& recommendations, const std::string& tag) {
    return std::any_of(recommendations.begin(), recommendations.end(), [&tag](const Recommendation& item) {
        return item.tag == tag;
    });
}

const Recommendation* find_tag(const std::vector<Recommendation>& recommendations, const std::string& tag) {
    const auto it =
        std::find_if(recommendations.begin(), recommendations.end(), [&tag](const Recommendation& item) {
            return item.tag == tag;
        });
    return it == recommendations.end() ? nullptr : &*it;
}

} // namespace

TEST(AdvisorTest, HealthyFingerprintProducesNoRecommendations) {
    EXPECT_TRUE(diagnose(make_healthy_fingerprint()).empty());
}

TEST(AdvisorTest, TransferDominatedTriggersCriticalRecommendation) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.transfer_compute_ratio = 5.0;

    const std::vector<Recommendation> recommendations = diagnose(fingerprint);
    const Recommendation* recommendation = find_tag(recommendations, "transfer_dominated");

    ASSERT_NE(recommendation, nullptr);
    EXPECT_EQ(recommendation->severity, "critical");
}

TEST(AdvisorTest, LowOccupancyRequiresHighRegisterUsage) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.occupancy = 0.3;
    fingerprint.num_regs = 48;
    EXPECT_TRUE(contains_tag(diagnose(fingerprint), "low_occupancy"));

    fingerprint.num_regs = 16;
    EXPECT_FALSE(contains_tag(diagnose(fingerprint), "low_occupancy"));
}

TEST(AdvisorTest, LocalMemoryPressureTriggersWhenLocalMemoryIsNonZero) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.local_size_bytes = 32U;
    EXPECT_TRUE(contains_tag(diagnose(fingerprint), "local_memory_pressure"));

    fingerprint.local_size_bytes = 0U;
    EXPECT_FALSE(contains_tag(diagnose(fingerprint), "local_memory_pressure"));
}

TEST(AdvisorTest, BandwidthBoundTriggersWhenUtilizationIsHigh) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.bandwidth_utilization = 0.9;

    const std::vector<Recommendation> recommendations = diagnose(fingerprint);
    const Recommendation* recommendation = find_tag(recommendations, "bandwidth_bound");
    ASSERT_NE(recommendation, nullptr);
    EXPECT_EQ(recommendation->severity, "info");
}

TEST(AdvisorTest, UnstableTimingTriggersWarning) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.cv = 0.25;

    const std::vector<Recommendation> recommendations = diagnose(fingerprint);
    const Recommendation* recommendation = find_tag(recommendations, "unstable_timing");
    ASSERT_NE(recommendation, nullptr);
    EXPECT_EQ(recommendation->severity, "warning");
}

TEST(AdvisorTest, BlockSensitivityTriggersInfoRecommendation) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.block_sensitivity = 2.0;
    EXPECT_TRUE(contains_tag(diagnose(fingerprint), "block_sensitive"));
}

TEST(AdvisorTest, SuperlinearScalingRequiresPositiveAboveThresholdExponent) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.scaling_exponent = 1.5;
    EXPECT_TRUE(contains_tag(diagnose(fingerprint), "superlinear_scaling"));

    fingerprint.scaling_exponent = 0.0;
    EXPECT_FALSE(contains_tag(diagnose(fingerprint), "superlinear_scaling"));
}

TEST(AdvisorTest, WellUtilizedRequiresNoOtherFindings) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.transfer_compute_ratio = 0.1;
    fingerprint.occupancy = 0.7;
    fingerprint.cv = 0.05;
    fingerprint.block_sensitivity = 1.1;

    EXPECT_TRUE(contains_tag(diagnose(fingerprint), "well_utilized"));

    fingerprint.local_size_bytes = 32U;
    EXPECT_FALSE(contains_tag(diagnose(fingerprint), "well_utilized"));
}

TEST(AdvisorTest, MultipleFindingsAreReportedTogether) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.transfer_compute_ratio = 5.0;
    fingerprint.cv = 0.3;
    fingerprint.block_sensitivity = 2.0;

    const std::vector<Recommendation> recommendations = diagnose(fingerprint);
    EXPECT_EQ(recommendations.size(), 3U);
    EXPECT_TRUE(contains_tag(recommendations, "transfer_dominated"));
    EXPECT_TRUE(contains_tag(recommendations, "unstable_timing"));
    EXPECT_TRUE(contains_tag(recommendations, "block_sensitive"));
}

TEST(AdvisorTest, RecommendationFieldsAreNonEmpty) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.transfer_compute_ratio = 5.0;

    const std::vector<Recommendation> recommendations = diagnose(fingerprint);
    ASSERT_EQ(recommendations.size(), 1U);
    EXPECT_FALSE(recommendations.front().tag.empty());
    EXPECT_FALSE(recommendations.front().severity.empty());
    EXPECT_FALSE(recommendations.front().summary.empty());
    EXPECT_FALSE(recommendations.front().suggestion.empty());
}

TEST(AdvisorTest, TransferDominatedUsesStrictGreaterThanThreshold) {
    KernelFingerprint fingerprint = make_healthy_fingerprint();
    fingerprint.transfer_compute_ratio = 3.0;
    EXPECT_FALSE(contains_tag(diagnose(fingerprint), "transfer_dominated"));

    fingerprint.transfer_compute_ratio = 3.01;
    EXPECT_TRUE(contains_tag(diagnose(fingerprint), "transfer_dominated"));
}

TEST(AdvisorTest, NegativeMetricsDoNotCrashOrTriggerPositiveFindings) {
    KernelFingerprint fingerprint;
    fingerprint.transfer_compute_ratio = -1.0;
    fingerprint.occupancy = -0.1;
    fingerprint.bandwidth_utilization = -0.5;
    fingerprint.cv = -0.1;
    fingerprint.block_sensitivity = -1.0;
    fingerprint.scaling_exponent = -2.0;
    fingerprint.num_regs = -4;

    EXPECT_TRUE(diagnose(fingerprint).empty());
}

} // namespace cuda_test::analysis

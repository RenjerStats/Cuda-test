#include "cuda_test/benchmark/stats.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace cuda_test::benchmark {
namespace {

TEST(ComputeStatsTest, ConstantSeriesProducesZeroSpread) {
    const std::vector<double> samples(30, 5.0);

    const core::RunStats stats = compute_stats(samples);

    EXPECT_DOUBLE_EQ(stats.mean_ms, 5.0);
    EXPECT_DOUBLE_EQ(stats.median_ms, 5.0);
    EXPECT_DOUBLE_EQ(stats.p95_ms, 5.0);
    EXPECT_DOUBLE_EQ(stats.ci95_low, 5.0);
    EXPECT_DOUBLE_EQ(stats.ci95_high, 5.0);
    EXPECT_DOUBLE_EQ(stats.cv, 0.0);
}

TEST(ComputeStatsTest, LinearSeriesMatchesExpectedStatistics) {
    std::vector<double> samples;
    samples.reserve(30);
    for (int value = 1; value <= 30; ++value) {
        samples.push_back(static_cast<double>(value));
    }

    const core::RunStats stats = compute_stats(samples);

    EXPECT_NEAR(stats.mean_ms, 15.5, 1e-9);
    EXPECT_NEAR(stats.median_ms, 15.5, 1e-9);
    EXPECT_NEAR(stats.p95_ms, 28.55, 1e-9);
    EXPECT_NEAR(stats.ci95_low, 12.349740751409, 1e-9);
    EXPECT_NEAR(stats.ci95_high, 18.650259248591, 1e-9);
    EXPECT_NEAR(stats.cv, 0.567961834247065, 1e-9);
}

TEST(ComputeStatsTest, SingleSampleCollapsesStatistics) {
    const std::vector<double> samples{7.5};

    const core::RunStats stats = compute_stats(samples);

    EXPECT_DOUBLE_EQ(stats.mean_ms, 7.5);
    EXPECT_DOUBLE_EQ(stats.median_ms, 7.5);
    EXPECT_DOUBLE_EQ(stats.p95_ms, 7.5);
    EXPECT_DOUBLE_EQ(stats.ci95_low, 7.5);
    EXPECT_DOUBLE_EQ(stats.ci95_high, 7.5);
    EXPECT_DOUBLE_EQ(stats.cv, 0.0);
}

TEST(ComputeStatsTest, RejectsEmptySamples) {
    EXPECT_THROW((void)compute_stats({}), std::invalid_argument);
}

} // namespace
} // namespace cuda_test::benchmark

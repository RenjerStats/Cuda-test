#include "cuda_test/benchmark/benchmark_runner.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace cuda_test::benchmark {
namespace {

TEST(BenchmarkRunnerTest, ExcludesWarmupIterationsFromMeasuredSamples) {
    const BenchmarkRunner runner({2, 3});
    int invocation_count = 0;

    const BenchmarkResult result = runner.run([&invocation_count]() {
        ++invocation_count;
        const double value = static_cast<double>(invocation_count);
        return core::ProfilingBreakdown{value, value * 10.0, value * 100.0, value * 1000.0};
    });

    ASSERT_EQ(result.samples.size(), 3U);
    EXPECT_EQ(invocation_count, 5);
    EXPECT_DOUBLE_EQ(result.samples[0].kernel_ms, 30.0);
    EXPECT_DOUBLE_EQ(result.samples[1].kernel_ms, 40.0);
    EXPECT_DOUBLE_EQ(result.samples[2].kernel_ms, 50.0);
    EXPECT_DOUBLE_EQ(result.h2d_stats.mean_ms, 4.0);
    EXPECT_DOUBLE_EQ(result.kernel_stats.median_ms, 40.0);
    EXPECT_DOUBLE_EQ(result.total_stats.mean_ms, 4000.0);
}

TEST(BenchmarkRunnerTest, AggregatesStageSpecificStatistics) {
    const BenchmarkRunner runner({0, 3});
    const std::vector<core::ProfilingBreakdown> sequence{
        {1.0, 10.0, 100.0, 1000.0},
        {2.0, 20.0, 200.0, 2000.0},
        {8.0, 80.0, 800.0, 8000.0},
    };
    std::size_t index = 0;

    const BenchmarkResult result = runner.run([&sequence, &index]() {
        return sequence.at(index++);
    });

    ASSERT_EQ(result.samples.size(), sequence.size());
    EXPECT_DOUBLE_EQ(result.h2d_stats.median_ms, 2.0);
    EXPECT_DOUBLE_EQ(result.kernel_stats.median_ms, 20.0);
    EXPECT_DOUBLE_EQ(result.d2h_stats.mean_ms, 366.66666666666669);
    EXPECT_DOUBLE_EQ(result.total_stats.p95_ms, 7400.0);
}

TEST(BenchmarkRunnerTest, RejectsZeroMeasuredRuns) {
    EXPECT_THROW((void)BenchmarkRunner({0, 0}), std::invalid_argument);
}

TEST(BenchmarkRunnerTest, RejectsNegativeWarmupRuns) {
    EXPECT_THROW((void)BenchmarkRunner({-1, 1}), std::invalid_argument);
}

TEST(BenchmarkRunnerTest, PropagatesMeasuredCallableExceptions) {
    const BenchmarkRunner runner({1, 2});

    EXPECT_THROW(
        (void)runner.run([]() -> core::ProfilingBreakdown {
            throw std::runtime_error("benchmark failure");
        }),
        std::runtime_error);
}

} // namespace
} // namespace cuda_test::benchmark

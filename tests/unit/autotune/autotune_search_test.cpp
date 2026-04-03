#include "cuda_test/autotune/search.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cuda_test::autotune {
namespace {

using CandidateKey = std::pair<unsigned int, unsigned int>;

struct SyntheticMeasuredRun {
    std::map<CandidateKey, std::vector<double>> kernel_sequences;
    std::map<CandidateKey, std::size_t> offsets;
    int invocation_count = 0;

    core::ProfilingBreakdown operator()(const core::KernelLaunchConfig& config) {
        ++invocation_count;
        const CandidateKey key{config.block.x, config.grid.x};

        auto sequence_it = kernel_sequences.find(key);
        if (sequence_it == kernel_sequences.end()) {
            throw std::out_of_range("Missing synthetic sequence for candidate");
        }

        std::size_t& offset = offsets[key];
        if (offset >= sequence_it->second.size()) {
            throw std::out_of_range("Synthetic sequence exhausted for candidate");
        }

        const double kernel_ms = sequence_it->second[offset++];
        return core::ProfilingBreakdown{
            kernel_ms * 0.1,
            kernel_ms,
            kernel_ms * 0.2,
            kernel_ms * 1.3,
        };
    }
};

TEST(AutoTuneSearchTest, SingleCandidateUsesComputedWaveAdjustedGrid) {
    AutoTuneSpec spec;
    spec.block_sizes = {64};
    spec.grid_wave_multipliers = {2};
    spec.warmup_runs = 0;
    spec.measure_runs = 3;

    SyntheticMeasuredRun measured_run;
    measured_run.kernel_sequences[{64U, 6U}] = {4.0, 4.0, 4.0};

    const AutoTuneResult result = tune_kernel(
        spec,
        130,
        measured_run,
        [](const core::KernelLaunchConfig&) { return true; });

    ASSERT_EQ(result.all_candidates.size(), 1U);
    EXPECT_EQ(result.best.block.x, 64U);
    EXPECT_EQ(result.best.grid.x, 6U);
    EXPECT_DOUBLE_EQ(result.stats.median_ms, 4.0);
    EXPECT_NE(result.reason.find("only valid candidate"), std::string::npos);
}

TEST(AutoTuneSearchTest, ChoosesCandidateWithLowestKernelMedian) {
    AutoTuneSpec spec;
    spec.block_sizes = {64, 128, 256};
    spec.warmup_runs = 0;
    spec.measure_runs = 3;

    SyntheticMeasuredRun measured_run;
    measured_run.kernel_sequences[{64U, 4U}] = {5.0, 5.0, 5.0};
    measured_run.kernel_sequences[{128U, 2U}] = {1.0, 1.0, 1.0};
    measured_run.kernel_sequences[{256U, 1U}] = {3.0, 3.0, 3.0};

    const AutoTuneResult result = tune_kernel(
        spec,
        256,
        measured_run,
        [](const core::KernelLaunchConfig&) { return true; });

    EXPECT_EQ(result.best.block.x, 128U);
    EXPECT_DOUBLE_EQ(result.stats.median_ms, 1.0);
    EXPECT_NE(result.reason.find("lowest median_ms"), std::string::npos);
}

TEST(AutoTuneSearchTest, BreaksMedianTieWithLowerP95) {
    AutoTuneSpec spec;
    spec.block_sizes = {64, 128};
    spec.warmup_runs = 0;
    spec.measure_runs = 3;

    SyntheticMeasuredRun measured_run;
    measured_run.kernel_sequences[{64U, 2U}] = {1.0, 1.0, 10.0};
    measured_run.kernel_sequences[{128U, 1U}] = {1.0, 1.0, 2.0};

    const AutoTuneResult result = tune_kernel(
        spec,
        128,
        measured_run,
        [](const core::KernelLaunchConfig&) { return true; });

    EXPECT_EQ(result.best.block.x, 128U);
    EXPECT_NE(result.reason.find("p95_ms"), std::string::npos);
}

TEST(AutoTuneSearchTest, BreaksRemainingTieWithLowerCv) {
    AutoTuneSpec spec;
    spec.block_sizes = {64, 128};
    spec.warmup_runs = 0;
    spec.measure_runs = 3;

    SyntheticMeasuredRun measured_run;
    measured_run.kernel_sequences[{64U, 2U}] = {1.0, 2.0, 2.0};
    measured_run.kernel_sequences[{128U, 1U}] = {2.0, 2.0, 2.0};

    const AutoTuneResult result = tune_kernel(
        spec,
        128,
        measured_run,
        [](const core::KernelLaunchConfig&) { return true; });

    EXPECT_EQ(result.best.block.x, 128U);
    EXPECT_NE(result.reason.find("cv"), std::string::npos);
}

TEST(AutoTuneSearchTest, FiltersOversizedBlocksBeforeMeasurement) {
    AutoTuneSpec spec;
    spec.block_sizes = {64, 2048};
    spec.warmup_runs = 0;
    spec.measure_runs = 1;
    spec.max_threads_per_block = 1024;

    SyntheticMeasuredRun measured_run;
    measured_run.kernel_sequences[{64U, 4U}] = {3.0};

    const AutoTuneResult result = tune_kernel(
        spec,
        256,
        measured_run,
        [](const core::KernelLaunchConfig&) { return true; });

    ASSERT_EQ(result.all_candidates.size(), 1U);
    EXPECT_EQ(result.best.block.x, 64U);
    EXPECT_EQ(measured_run.invocation_count, 1);
}

TEST(AutoTuneSearchTest, RejectsSpecWithEmptyCandidateSpaceOrInvalidValues) {
    AutoTuneSpec empty_blocks;
    EXPECT_THROW(
        (void)tune_kernel(
            empty_blocks,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);

    AutoTuneSpec empty_waves;
    empty_waves.block_sizes = {64};
    empty_waves.grid_wave_multipliers.clear();
    EXPECT_THROW(
        (void)tune_kernel(
            empty_waves,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);

    AutoTuneSpec invalid_counts;
    invalid_counts.block_sizes = {64};
    invalid_counts.measure_runs = 0;
    EXPECT_THROW(
        (void)tune_kernel(
            invalid_counts,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);

    AutoTuneSpec negative_warmup;
    negative_warmup.block_sizes = {64};
    negative_warmup.warmup_runs = -1;
    EXPECT_THROW(
        (void)tune_kernel(
            negative_warmup,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);

    AutoTuneSpec invalid_block;
    invalid_block.block_sizes = {0};
    EXPECT_THROW(
        (void)tune_kernel(
            invalid_block,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);

    AutoTuneSpec invalid_wave;
    invalid_wave.block_sizes = {64};
    invalid_wave.grid_wave_multipliers = {0};
    EXPECT_THROW(
        (void)tune_kernel(
            invalid_wave,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);

    AutoTuneSpec invalid_thread_limit;
    invalid_thread_limit.block_sizes = {64};
    invalid_thread_limit.max_threads_per_block = 0;
    EXPECT_THROW(
        (void)tune_kernel(
            invalid_thread_limit,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::invalid_argument);
}

TEST(AutoTuneSearchTest, ThrowsWhenNoCandidatesSurviveFilteringOrValidation) {
    AutoTuneSpec spec;
    spec.block_sizes = {64, 128};
    spec.warmup_runs = 0;
    spec.measure_runs = 1;

    EXPECT_THROW(
        (void)tune_kernel(
            spec,
            128,
            [](const core::KernelLaunchConfig&) { return core::ProfilingBreakdown{}; },
            [](const core::KernelLaunchConfig&) { return false; }),
        std::runtime_error);
}

TEST(AutoTuneSearchTest, PropagatesMeasuredRunExceptions) {
    AutoTuneSpec spec;
    spec.block_sizes = {64};
    spec.warmup_runs = 0;
    spec.measure_runs = 1;

    EXPECT_THROW(
        (void)tune_kernel(
            spec,
            128,
            [](const core::KernelLaunchConfig&) -> core::ProfilingBreakdown {
                throw std::runtime_error("synthetic autotune failure");
            },
            [](const core::KernelLaunchConfig&) { return true; }),
        std::runtime_error);
}

} // namespace
} // namespace cuda_test::autotune

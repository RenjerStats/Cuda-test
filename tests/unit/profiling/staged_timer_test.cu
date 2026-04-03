#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/device_memory.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/profiling/staged_timer.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace cuda_test::profiling {
namespace {

__global__ void increment_kernel(const float* input, float* output, std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = input[index] + 1.0f;
    }
}

double coefficient_of_variation(const std::vector<double>& samples) {
    if (samples.empty()) {
        return 0.0;
    }

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double mean = sum / static_cast<double>(samples.size());
    if (mean == 0.0) {
        return 0.0;
    }

    double squared_sum = 0.0;
    for (const double sample : samples) {
        const double delta = sample - mean;
        squared_sum += delta * delta;
    }

    if (samples.size() == 1) {
        return 0.0;
    }

    const double variance = squared_sum / static_cast<double>(samples.size() - 1);
    return std::sqrt(variance) / mean;
}

class StagedTimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }
    }

    [[nodiscard]] std::vector<float> make_input(std::size_t size) const {
        std::vector<float> data(size);
        std::iota(data.begin(), data.end(), 1.0f);
        return data;
    }

    core::ProfilingBreakdown run_with_named_wrappers(StagedTimer& timer,
                                                     std::size_t size,
                                                     int kernel_repeats = 1) {
        const std::vector<float> input = make_input(size);
        core::DeviceMemory<float> device_input(size);
        core::DeviceMemory<float> device_output(size);
        const dim3 block(64, 1, 1);
        const dim3 grid(static_cast<unsigned int>((size + block.x - 1) / block.x), 1, 1);

        timer.start_total();
        timer.start_h2d();
        device_input.copy_from_host(input);
        timer.stop_h2d();

        timer.start_kernel();
        for (int iteration = 0; iteration < kernel_repeats; ++iteration) {
            increment_kernel<<<grid, block>>>(device_input.data(), device_output.data(), size);
        }
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        const std::vector<float> output = device_output.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();

        EXPECT_EQ(output.size(), input.size());
        if (!output.empty() && output.size() == input.size()) {
            EXPECT_FLOAT_EQ(output.front(), input.front() + 1.0f);
            EXPECT_FLOAT_EQ(output.back(), input.back() + 1.0f);
        }
        return timer.breakdown();
    }
};

TEST_F(StagedTimerTest, DefaultBreakdownIsZero) {
    const StagedTimer timer;
    const core::ProfilingBreakdown breakdown = timer.breakdown();

    EXPECT_DOUBLE_EQ(breakdown.h2d_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.kernel_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.d2h_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.total_ms, 0.0);
}

TEST_F(StagedTimerTest, SmokeMeasuresAllStages) {
    StagedTimer timer;
    const core::ProfilingBreakdown breakdown = run_with_named_wrappers(timer, 256);

    EXPECT_GE(breakdown.h2d_ms, 0.0);
    EXPECT_GE(breakdown.kernel_ms, 0.0);
    EXPECT_GE(breakdown.d2h_ms, 0.0);
    EXPECT_GE(breakdown.total_ms, breakdown.kernel_ms);
}

TEST_F(StagedTimerTest, ResetClearsBreakdown) {
    StagedTimer timer;
    (void)run_with_named_wrappers(timer, 256);

    timer.reset();
    const core::ProfilingBreakdown breakdown = timer.breakdown();

    EXPECT_DOUBLE_EQ(breakdown.h2d_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.kernel_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.d2h_ms, 0.0);
    EXPECT_DOUBLE_EQ(breakdown.total_ms, 0.0);
}

TEST_F(StagedTimerTest, ReuseProducesFreshTimings) {
    StagedTimer timer;
    const core::ProfilingBreakdown first = run_with_named_wrappers(timer, 256);
    timer.reset();
    const core::ProfilingBreakdown second = run_with_named_wrappers(timer, 256);

    EXPECT_GE(first.total_ms, first.kernel_ms);
    EXPECT_GE(second.total_ms, second.kernel_ms);
    EXPECT_GE(second.h2d_ms, 0.0);
}

TEST_F(StagedTimerTest, GenericStageApiRejectsInvalidOrdering) {
    StagedTimer timer;

    EXPECT_THROW(timer.stop(Stage::kernel), std::logic_error);

    timer.start(Stage::h2d);
    EXPECT_THROW(timer.start(Stage::h2d), std::logic_error);
}

TEST_F(StagedTimerTest, KernelTimingRemainsReasonablyStable) {
    StagedTimer timer;
    std::vector<double> kernel_samples;
    kernel_samples.reserve(10);
    constexpr std::size_t stability_size = 1u << 20;
    constexpr int kernel_repeats = 32;

    (void)run_with_named_wrappers(timer, stability_size, kernel_repeats);
    timer.reset();

    for (int iteration = 0; iteration < 10; ++iteration) {
        const core::ProfilingBreakdown breakdown =
            run_with_named_wrappers(timer, stability_size, kernel_repeats);
        kernel_samples.push_back(breakdown.kernel_ms);
        timer.reset();
    }

    EXPECT_LT(coefficient_of_variation(kernel_samples), 0.5);
}

} // namespace
} // namespace cuda_test::profiling

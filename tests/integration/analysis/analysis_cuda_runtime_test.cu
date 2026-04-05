#include "cuda_test/analysis/kernel_attributes.hpp"
#include "cuda_test/analysis/memory_info.hpp"
#include "cuda_test/analysis/occupancy.hpp"
#include "cuda_test/core/device_info.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace cuda_test::analysis {
namespace {

__global__ void vector_add_kernel(const float* lhs, const float* rhs, float* output, std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = lhs[index] + rhs[index];
    }
}

class AnalysisCudaRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }

        CUDA_CHECK(cudaSetDevice(0));
    }
};

TEST_F(AnalysisCudaRuntimeTest, EstimateOccupancyReturnsWarpBasedMetrics) {
    const OccupancyInfo info =
        estimate_occupancy(reinterpret_cast<const void*>(vector_add_kernel), 128, 0, 0);

    EXPECT_GT(info.active_blocks_per_sm, 0);
    EXPECT_GT(info.active_warps_per_sm, 0);
    EXPECT_GT(info.max_warps_per_sm, 0);
    EXPECT_GT(info.occupancy_ratio, 0.0);
    EXPECT_LE(info.occupancy_ratio, 1.0);
}

TEST_F(AnalysisCudaRuntimeTest, EstimateOccupancySupportsVerySmallBlocks) {
    const OccupancyInfo info =
        estimate_occupancy(reinterpret_cast<const void*>(vector_add_kernel), 1, 0, 0);

    EXPECT_GT(info.active_blocks_per_sm, 0);
    EXPECT_GT(info.active_warps_per_sm, 0);
    EXPECT_GT(info.max_warps_per_sm, 0);
}

TEST_F(AnalysisCudaRuntimeTest, EstimateOccupancySupportsMaximumBlockSize) {
    const int max_threads_per_block = core::get_device_info(0).max_threads_per_block;
    const OccupancyInfo info =
        estimate_occupancy(reinterpret_cast<const void*>(vector_add_kernel), max_threads_per_block, 0, 0);

    EXPECT_GT(info.active_blocks_per_sm, 0);
    EXPECT_GT(info.occupancy_ratio, 0.0);
    EXPECT_LE(info.occupancy_ratio, 1.0);
}

TEST_F(AnalysisCudaRuntimeTest, SuggestBlockSizeReturnsWarpAlignedValue) {
    const int block_size = suggest_block_size(vector_add_kernel, 0, 0, 0);

    EXPECT_GT(block_size, 0);
    EXPECT_EQ(block_size % 32, 0);
}

TEST_F(AnalysisCudaRuntimeTest, KernelAttributesExposeRegistersAndLimits) {
    const KernelAttributes attributes =
        get_kernel_attributes(reinterpret_cast<const void*>(vector_add_kernel));

    EXPECT_GT(attributes.num_regs, 0);
    EXPECT_GT(attributes.max_threads_per_block, 0);
    EXPECT_GE(attributes.max_dynamic_shared_size_bytes, 0);
}

TEST_F(AnalysisCudaRuntimeTest, MemoryPressureReturnsConsistentRanges) {
    const MemoryPressure pressure = query_memory_pressure(0);

    EXPECT_GT(pressure.total_bytes, 0U);
    EXPECT_LE(pressure.free_bytes, pressure.total_bytes);
    EXPECT_GE(pressure.usage_ratio, 0.0);
    EXPECT_LE(pressure.usage_ratio, 1.0);
}

TEST(AnalysisCudaRuntimeNegativeTest, EstimateOccupancyRejectsNullKernel) {
    EXPECT_THROW((void)estimate_occupancy(nullptr, 128, 0, 0), std::invalid_argument);
}

TEST(AnalysisCudaRuntimeNegativeTest, EstimateOccupancyRejectsInvalidBlockSize) {
    EXPECT_THROW((void)estimate_occupancy(reinterpret_cast<const void*>(vector_add_kernel), 0, 0, 0),
                 std::invalid_argument);
}

TEST(AnalysisCudaRuntimeNegativeTest, GetKernelAttributesRejectsNullKernel) {
    EXPECT_THROW((void)get_kernel_attributes(nullptr), std::invalid_argument);
}

TEST(AnalysisCudaRuntimeNegativeTest, QueryMemoryPressureRejectsInvalidDevice) {
    EXPECT_THROW((void)query_memory_pressure(999), std::out_of_range);
}

} // namespace
} // namespace cuda_test::analysis

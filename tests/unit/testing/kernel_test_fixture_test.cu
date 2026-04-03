#include "cuda_test/testing/kernel_test_fixture.hpp"
#include "cuda_test/testing/validation.hpp"
#include "tests/fixtures/vector_add_fixture.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace cuda_test::testing {
namespace {

__global__ void vector_add_kernel(const float* lhs, const float* rhs, float* output, std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = lhs[index] + rhs[index];
    }
}

class VectorAddKernelTest : public KernelTestFixture {
protected:
    void SetUp() override {
        KernelTestFixture::SetUp();

        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }
    }
};

TEST_F(VectorAddKernelTest, Computes1DLaunchConfig) {
    const core::KernelLaunchConfig config = make_1d_launch_config(1024, 128);

    EXPECT_EQ(config.grid.x, 8U);
    EXPECT_EQ(config.block.x, 128U);
    EXPECT_EQ(config.grid.y, 1U);
    EXPECT_EQ(config.block.y, 1U);
}

TEST_F(VectorAddKernelTest, RejectsInvalidInputs) {
    EXPECT_THROW((void)make_1d_launch_config(1024, 0), std::invalid_argument);
    EXPECT_THROW(select_device(core::device_count()), std::out_of_range);
}

TEST_F(VectorAddKernelTest, VectorAddMatchesHostReference) {
    const auto fixture = tests::fixtures::make_vector_add_fixture(1024);
    core::DeviceMemory<float> lhs_device(fixture.lhs.size());
    core::DeviceMemory<float> rhs_device(fixture.rhs.size());
    core::DeviceMemory<float> output_device(fixture.expected.size());

    lhs_device.copy_from_host(fixture.lhs);
    rhs_device.copy_from_host(fixture.rhs);

    const core::KernelLaunchConfig config = make_1d_launch_config(fixture.expected.size(), 128);
    vector_add_kernel<<<config.grid, config.block>>>(
        lhs_device.data(),
        rhs_device.data(),
        output_device.data(),
        fixture.expected.size());
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    const std::vector<float> output = output_device.copy_to_host();
    expect_array_near(output.data(), fixture.expected.data(), fixture.expected.size(), 1e-5f);
}

} // namespace
} // namespace cuda_test::testing

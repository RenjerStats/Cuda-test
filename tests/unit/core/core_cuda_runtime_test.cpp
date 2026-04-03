#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/device_memory.hpp"

#include <gtest/gtest.h>

#include <numeric>
#include <stdexcept>
#include <vector>

namespace cuda_test::core {
namespace {

TEST(CheckCudaTest, ThrowsWithContext) {
    try {
        check_cuda(cudaErrorInvalidValue, "cudaDoThing()", "core_cuda_runtime_test.cpp", 42);
        FAIL() << "Expected check_cuda to throw";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("cudaDoThing()"), std::string::npos);
        EXPECT_NE(message.find("core_cuda_runtime_test.cpp:42"), std::string::npos);
    }
}

TEST(DeviceMemoryTest, ZeroSizeBufferIsValid) {
    const DeviceMemory<float> memory;

    EXPECT_EQ(memory.size(), 0U);
    EXPECT_EQ(memory.data(), nullptr);
    EXPECT_TRUE(memory.empty());
}

TEST(DeviceMemoryTest, AllocatesAndCopiesRoundTrip) {
    std::vector<float> input(1024);
    std::iota(input.begin(), input.end(), 1.0f);

    DeviceMemory<float> memory(input.size());
    ASSERT_EQ(memory.size(), input.size());
    ASSERT_NE(memory.data(), nullptr);

    memory.copy_from_host(input);
    const std::vector<float> output = memory.copy_to_host();

    EXPECT_EQ(output, input);
}

TEST(DeviceMemoryTest, SupportsMoveSemantics) {
    std::vector<float> input(16);
    std::iota(input.begin(), input.end(), 1.0f);

    DeviceMemory<float> source(input.size());
    source.copy_from_host(input);

    DeviceMemory<float> moved(std::move(source));
    EXPECT_EQ(source.size(), 0U);
    EXPECT_EQ(source.data(), nullptr);
    EXPECT_EQ(moved.size(), input.size());

    DeviceMemory<float> assigned;
    assigned = std::move(moved);
    EXPECT_EQ(moved.size(), 0U);
    EXPECT_EQ(moved.data(), nullptr);
    EXPECT_EQ(assigned.copy_to_host(), input);
}

TEST(DeviceMemoryTest, RejectsCopySizeMismatch) {
    DeviceMemory<float> memory(4);
    std::vector<float> input(3, 1.0f);

    EXPECT_THROW(memory.copy_from_host(input.data(), input.size()), std::invalid_argument);
}

TEST(DeviceInfoTest, ReportsVisibleDevice) {
    const int count = device_count();
    if (count <= 0) {
        GTEST_SKIP() << "No CUDA device is visible to the runtime";
    }

    EXPECT_TRUE(device_exists(0));

    const DeviceInfo info = get_device_info(0);
    EXPECT_FALSE(info.name.empty());
    EXPECT_GT(info.max_threads_per_block, 0);
    EXPECT_GT(info.total_global_memory, 0U);
}

TEST(DeviceInfoTest, RejectsInvalidDeviceId) {
    EXPECT_FALSE(device_exists(-1));
    EXPECT_THROW((void)get_device_info(device_count()), std::out_of_range);
}

} // namespace
} // namespace cuda_test::core

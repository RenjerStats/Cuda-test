#pragma once

#include <gtest/gtest.h>

#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/device_memory.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/core/types.hpp"

#include <cstddef>
#include <stdexcept>

namespace cuda_test::testing {

class KernelTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        if (core::device_count() > 0) {
            select_device(0);
        }
    }

    void select_device(int device_id) {
        if (!core::device_exists(device_id)) {
            throw std::out_of_range("Requested CUDA device does not exist");
        }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        CUDA_CHECK(cudaSetDevice(device_id));
        selected_device_id_ = device_id;
#else
        (void)device_id;
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    [[nodiscard]] int selected_device_id() const noexcept {
        return selected_device_id_;
    }

    [[nodiscard]] core::KernelLaunchConfig make_1d_launch_config(std::size_t size,
                                                                 unsigned int block_size) const {
        if (block_size == 0) {
            throw std::invalid_argument("Block size must be greater than zero");
        }

        const unsigned int grid_x =
            size == 0 ? 0U : static_cast<unsigned int>((size + block_size - 1) / block_size);

        core::KernelLaunchConfig config;
        config.grid = dim3(grid_x, 1, 1);
        config.block = dim3(block_size, 1, 1);
        config.device_id = selected_device_id_;
        return config;
    }

    template <typename T>
    [[nodiscard]] core::DeviceMemory<T> make_device_buffer(std::size_t size) const {
        return core::DeviceMemory<T>(size);
    }

private:
    int selected_device_id_ = 0;
};

} // namespace cuda_test::testing

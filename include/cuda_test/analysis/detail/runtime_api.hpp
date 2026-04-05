#pragma once

#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/error.hpp"

#include <stdexcept>
#include <string>

namespace cuda_test::analysis::detail {

[[noreturn]] inline void throw_cuda_unavailable(const char* api_name) {
    const std::string name = api_name != nullptr ? api_name : "analysis runtime API";
    throw std::runtime_error(name + " requires CUDA support");
}

inline void validate_device_id(int device_id) {
    if (!core::device_exists(device_id)) {
        throw std::out_of_range("Requested CUDA device does not exist");
    }
}

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA

class ScopedDevice {
public:
    explicit ScopedDevice(int device_id) : previous_device_(0), restore_needed_(false) {
        validate_device_id(device_id);

        CUDA_CHECK(cudaGetDevice(&previous_device_));
        if (previous_device_ != device_id) {
            CUDA_CHECK(cudaSetDevice(device_id));
            restore_needed_ = true;
        }
    }

    ScopedDevice(const ScopedDevice&) = delete;
    ScopedDevice& operator=(const ScopedDevice&) = delete;

    ~ScopedDevice() noexcept {
        if (restore_needed_) {
            (void)cudaSetDevice(previous_device_);
        }
    }

private:
    int previous_device_;
    bool restore_needed_;
};

inline cudaDeviceProp query_device_properties(int device_id) {
    validate_device_id(device_id);

    cudaDeviceProp properties{};
    CUDA_CHECK(cudaGetDeviceProperties(&properties, device_id));
    return properties;
}

inline int query_device_attribute(int device_id, cudaDeviceAttr attribute) {
    validate_device_id(device_id);

    int value = 0;
    CUDA_CHECK(cudaDeviceGetAttribute(&value, attribute, device_id));
    return value;
}

#endif

} // namespace cuda_test::analysis::detail

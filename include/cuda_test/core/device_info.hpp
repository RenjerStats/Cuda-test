#pragma once

#include "cuda_test/core/detail/cuda_compat.hpp"
#include "cuda_test/core/error.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace cuda_test::core {

struct DeviceInfo {
    int device_id = 0;
    std::string name;
    int major = 0;
    int minor = 0;
    int multi_processor_count = 0;
    int max_threads_per_block = 0;
    std::size_t total_global_memory = 0;
};

inline int device_count() {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    int count = 0;
    const cudaError_t error = cudaGetDeviceCount(&count);
    if (error == cudaErrorNoDevice) {
        return 0;
    }

    check_cuda(error, "cudaGetDeviceCount(&count)", __FILE__, __LINE__);
    return count;
#else
    return 0;
#endif
}

inline bool device_exists(int device_id) {
    return device_id >= 0 && device_id < device_count();
}

inline DeviceInfo get_device_info(int device_id) {
    if (!device_exists(device_id)) {
        throw std::out_of_range("Requested CUDA device does not exist");
    }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    cudaDeviceProp props{};
    check_cuda(cudaGetDeviceProperties(&props, device_id),
               "cudaGetDeviceProperties(&props, device_id)",
               __FILE__,
               __LINE__);

    DeviceInfo info;
    info.device_id = device_id;
    info.name = props.name;
    info.major = props.major;
    info.minor = props.minor;
    info.multi_processor_count = props.multiProcessorCount;
    info.max_threads_per_block = props.maxThreadsPerBlock;
    info.total_global_memory = props.totalGlobalMem;
    return info;
#else
    (void)device_id;
    throw std::runtime_error("CUDA support is disabled");
#endif
}

} // namespace cuda_test::core

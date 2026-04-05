#pragma once

#include "cuda_test/analysis/detail/runtime_api.hpp"

#include <cstddef>
#include <stdexcept>

namespace cuda_test::analysis {

struct KernelAttributes {
    int num_regs = 0;
    std::size_t shared_size_bytes = 0;
    std::size_t const_size_bytes = 0;
    std::size_t local_size_bytes = 0;
    int max_threads_per_block = 0;
    int max_dynamic_shared_size_bytes = 0;
    int ptx_version = 0;
    int binary_version = 0;
};

inline KernelAttributes get_kernel_attributes(const void* kernel_func) {
    if (kernel_func == nullptr) {
        throw std::invalid_argument("get_kernel_attributes requires a non-null kernel function");
    }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    cudaFuncAttributes attributes{};
    CUDA_CHECK(cudaFuncGetAttributes(&attributes, kernel_func));

    KernelAttributes info;
    info.num_regs = attributes.numRegs;
    info.shared_size_bytes = static_cast<std::size_t>(attributes.sharedSizeBytes);
    info.const_size_bytes = static_cast<std::size_t>(attributes.constSizeBytes);
    info.local_size_bytes = static_cast<std::size_t>(attributes.localSizeBytes);
    info.max_threads_per_block = attributes.maxThreadsPerBlock;
    info.max_dynamic_shared_size_bytes = attributes.maxDynamicSharedSizeBytes;
    info.ptx_version = attributes.ptxVersion;
    info.binary_version = attributes.binaryVersion;
    return info;
#else
    detail::throw_cuda_unavailable("get_kernel_attributes");
#endif
}

} // namespace cuda_test::analysis

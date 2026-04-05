#pragma once

#include "cuda_test/analysis/detail/runtime_api.hpp"

#include <algorithm>
#include <cstddef>

namespace cuda_test::analysis {

struct MemoryPressure {
    std::size_t free_bytes = 0;
    std::size_t total_bytes = 0;
    double usage_ratio = 0.0;
};

inline MemoryPressure query_memory_pressure(int device_id = 0) {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    detail::ScopedDevice scoped_device(device_id);

    std::size_t free_bytes = 0;
    std::size_t total_bytes = 0;
    CUDA_CHECK(cudaMemGetInfo(&free_bytes, &total_bytes));

    MemoryPressure pressure;
    pressure.free_bytes = free_bytes;
    pressure.total_bytes = total_bytes;
    pressure.usage_ratio = total_bytes > 0U
                               ? std::clamp(static_cast<double>(total_bytes - free_bytes) /
                                                static_cast<double>(total_bytes),
                                            0.0,
                                            1.0)
                               : 0.0;
    return pressure;
#else
    (void)device_id;
    detail::throw_cuda_unavailable("query_memory_pressure");
#endif
}

} // namespace cuda_test::analysis

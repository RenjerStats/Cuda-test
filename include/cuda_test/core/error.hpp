#pragma once

#include "cuda_test/core/detail/cuda_compat.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace cuda_test::core {

namespace detail {

inline std::string describe_cuda_error(cudaError_t error) {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    const char* name = cudaGetErrorName(error);
    const char* text = cudaGetErrorString(error);

    std::ostringstream stream;
    stream << (name != nullptr ? name : "cudaErrorUnknown");
    if (text != nullptr) {
        stream << ": " << text;
    }
    return stream.str();
#else
    std::ostringstream stream;
    stream << "CUDA runtime unavailable (error code " << error << ")";
    return stream.str();
#endif
}

} // namespace detail

inline void check_cuda(cudaError_t error, const char* expr, const char* file, int line) {
    if (error == cudaSuccess) {
        return;
    }

    std::ostringstream stream;
    stream << "CUDA call failed: " << (expr != nullptr ? expr : "<unknown expr>") << " at "
           << (file != nullptr ? file : "<unknown file>") << ":" << line << " -> "
           << detail::describe_cuda_error(error);
    throw std::runtime_error(stream.str());
}

} // namespace cuda_test::core

#define CUDA_CHECK(expr) ::cuda_test::core::check_cuda((expr), #expr, __FILE__, __LINE__)

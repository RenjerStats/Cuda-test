#pragma once

/// @file cuda_test.hpp
/// Umbrella header for the cuda_test library.
///
/// Individual module headers are included here as they become available.

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/benchmark/stats.hpp"
#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/device_memory.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/core/types.hpp"
#include "cuda_test/profiling/staged_timer.hpp"

namespace cuda_test {
namespace detail {
inline constexpr int version_major = 0;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;
} // namespace detail
} // namespace cuda_test

#pragma once

/// @file cuda_test.hpp
/// Umbrella header for the cuda_test library.
///
/// Individual module headers are included here as they become available.

#include "cuda_test/analysis/metrics.hpp"
#include "cuda_test/analysis/advisor.hpp"
#include "cuda_test/analysis/bandwidth.hpp"
#include "cuda_test/analysis/fingerprint.hpp"
#include "cuda_test/analysis/kernel_attributes.hpp"
#include "cuda_test/analysis/memory_info.hpp"
#include "cuda_test/analysis/occupancy.hpp"
#include "cuda_test/autotune/search.hpp"
#include "cuda_test/benchmark/benchmark_runner.hpp"
#include "cuda_test/benchmark/stats.hpp"
#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/device_memory.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/core/types.hpp"
#include "cuda_test/core/version.hpp"
#include "cuda_test/pipeline/kernel_descriptor.hpp"
#include "cuda_test/pipeline/pipeline.hpp"
#include "cuda_test/pipeline/pipeline_report.hpp"
#include "cuda_test/pipeline/suite.hpp"
#include "cuda_test/profiling/staged_timer.hpp"
#include "cuda_test/reporting/export.hpp"
#include "cuda_test/reporting/html_export.hpp"

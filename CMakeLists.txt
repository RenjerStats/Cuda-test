cmake_minimum_required(VERSION 3.26)

project(cuda_test VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CudaOptions)
include(Warnings)
include(Dependencies)

add_subdirectory(src)

if(CUDA_TEST_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(CUDA_TEST_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(CUDA_TEST_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

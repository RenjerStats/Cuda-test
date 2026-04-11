include_guard(GLOBAL)

option(CUDA_TEST_ENABLE_CUDA "Enable CUDA language and toolkit integration." OFF)
option(CUDA_TEST_BUILD_TESTS "Build unit and integration test targets." OFF)
option(CUDA_TEST_BUILD_BENCHMARKS "Build benchmark targets." OFF)
option(CUDA_TEST_BUILD_EXAMPLES "Build example targets." OFF)
option(CUDA_TEST_ENABLE_WARNINGS "Enable default compiler warnings." ON)

set(CUDA_TEST_HAS_CUDA OFF)

if(CUDA_TEST_ENABLE_CUDA)
  include(CheckLanguage)
  check_language(CUDA)

  if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CUDA_TEST_HAS_CUDA ON)
  else()
    message(STATUS "CUDA compiler not found; continuing with host-only scaffold.")
  endif()
endif()

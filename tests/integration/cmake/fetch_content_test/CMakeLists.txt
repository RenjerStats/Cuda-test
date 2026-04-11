cmake_minimum_required(VERSION 3.26)

project(cuda_test_fetch_content_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CUDA_TEST_GIT_REPOSITORY)
  message(FATAL_ERROR "CUDA_TEST_GIT_REPOSITORY must point to the cuda_test git repository")
endif()

if(NOT DEFINED CUDA_TEST_GIT_TAG)
  message(FATAL_ERROR "CUDA_TEST_GIT_TAG must identify the revision to fetch")
endif()

include(FetchContent)

set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_ENABLE_CUDA OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  cuda_test
  GIT_REPOSITORY "${CUDA_TEST_GIT_REPOSITORY}"
  GIT_TAG "${CUDA_TEST_GIT_TAG}"
)
FetchContent_MakeAvailable(cuda_test)

if(TARGET cuda_test::testing)
  message(FATAL_ERROR "cuda_test::testing must not exist when GTest is unavailable and tests are disabled")
endif()

add_executable(fetch_content_consumer main.cpp)
target_link_libraries(fetch_content_consumer PRIVATE cuda_test::cuda_test)

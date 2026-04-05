cmake_minimum_required(VERSION 3.26)

project(cuda_test_subdirectory_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CUDA_TEST_SOURCE_DIR)
  message(FATAL_ERROR "CUDA_TEST_SOURCE_DIR must point to the cuda_test source tree")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../common")

set(CUDA_TEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUDA_TEST_ENABLE_CUDA OFF CACHE BOOL "" FORCE)

if(CUDA_TEST_ENABLE_TESTING_COMPONENT)
  find_package(GTest REQUIRED)
endif()

add_subdirectory("${CUDA_TEST_SOURCE_DIR}" cuda_test-build)

add_executable(subdirectory_consumer main.cpp)
target_link_libraries(subdirectory_consumer PRIVATE cuda_test::cuda_test)

if(CUDA_TEST_ENABLE_TESTING_COMPONENT)
  if(NOT TARGET cuda_test::testing)
    message(FATAL_ERROR "cuda_test::testing target must exist when GTest is already available")
  endif()

  target_compile_definitions(subdirectory_consumer PRIVATE CUDA_TEST_USE_TESTING_COMPONENT=1)
  target_link_libraries(subdirectory_consumer PRIVATE cuda_test::testing)
elseif(TARGET cuda_test::testing)
  message(FATAL_ERROR "cuda_test::testing must not exist without GTest")
endif()

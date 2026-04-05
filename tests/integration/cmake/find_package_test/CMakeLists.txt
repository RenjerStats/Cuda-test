cmake_minimum_required(VERSION 3.26)

project(cuda_test_find_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../common")

if(NOT DEFINED CUDA_TEST_REQUIRED_VERSION)
  set(CUDA_TEST_REQUIRED_VERSION 0.2.0)
endif()

set(_cuda_test_components)
if(CUDA_TEST_ENABLE_TESTING_COMPONENT)
  list(APPEND _cuda_test_components testing)
endif()

if(_cuda_test_components)
  set(_cuda_test_component_args COMPONENTS ${_cuda_test_components})
else()
  set(_cuda_test_component_args)
endif()

if(CUDA_TEST_EXACT_VERSION)
  find_package(cuda_test ${CUDA_TEST_REQUIRED_VERSION} EXACT REQUIRED ${_cuda_test_component_args})
else()
  find_package(cuda_test ${CUDA_TEST_REQUIRED_VERSION} REQUIRED ${_cuda_test_component_args})
endif()

if(NOT TARGET cuda_test::cuda_test)
  message(FATAL_ERROR "cuda_test::cuda_test target is missing after find_package")
endif()

get_target_property(_cuda_test_type cuda_test::cuda_test TYPE)
if(NOT _cuda_test_type STREQUAL "INTERFACE_LIBRARY")
  message(FATAL_ERROR "cuda_test::cuda_test must be an INTERFACE library, got: ${_cuda_test_type}")
endif()

get_target_property(_cuda_test_include_dirs cuda_test::cuda_test INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _cuda_test_include_dirs)
  message(FATAL_ERROR "cuda_test::cuda_test must provide INTERFACE_INCLUDE_DIRECTORIES")
endif()

add_executable(find_package_consumer main.cpp)
target_link_libraries(find_package_consumer PRIVATE cuda_test::cuda_test)

if(CUDA_TEST_ENABLE_TESTING_COMPONENT)
  if(NOT TARGET cuda_test::testing)
    message(FATAL_ERROR "cuda_test::testing target is missing after requesting the testing component")
  endif()

  target_compile_definitions(find_package_consumer PRIVATE CUDA_TEST_USE_TESTING_COMPONENT=1)
  target_link_libraries(find_package_consumer PRIVATE cuda_test::testing)
endif()

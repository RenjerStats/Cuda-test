if(TARGET GTest::gtest)
  set(GTest_FOUND TRUE)
  return()
endif()

if(NOT DEFINED CUDA_TEST_GTEST_INCLUDE_DIR OR NOT EXISTS "${CUDA_TEST_GTEST_INCLUDE_DIR}/gtest/gtest.h")
  set(_cuda_test_gtest_error "CUDA_TEST_GTEST_INCLUDE_DIR must point to a valid GoogleTest include directory")
  if(GTest_FIND_REQUIRED)
    message(FATAL_ERROR "${_cuda_test_gtest_error}")
  endif()
  set(GTest_FOUND FALSE)
  return()
endif()

if(NOT DEFINED CUDA_TEST_GTEST_LIBRARY OR NOT EXISTS "${CUDA_TEST_GTEST_LIBRARY}")
  set(_cuda_test_gtest_error "CUDA_TEST_GTEST_LIBRARY must point to a valid GoogleTest library")
  if(GTest_FIND_REQUIRED)
    message(FATAL_ERROR "${_cuda_test_gtest_error}")
  endif()
  set(GTest_FOUND FALSE)
  return()
endif()

add_library(GTest::gtest UNKNOWN IMPORTED)
set_target_properties(GTest::gtest PROPERTIES
  IMPORTED_LOCATION "${CUDA_TEST_GTEST_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${CUDA_TEST_GTEST_INCLUDE_DIR}"
)

set(GTest_FOUND TRUE)

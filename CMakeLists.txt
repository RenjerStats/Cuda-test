cmake_minimum_required(VERSION 3.26)

project(cuda_test VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CUDA_TEST_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/cuda_test")

include(CudaOptions)
include(Warnings)
include(Dependencies)

add_library(cuda_test INTERFACE)
add_library(cuda_test::cuda_test ALIAS cuda_test)

target_compile_features(cuda_test INTERFACE cxx_std_17)
target_compile_definitions(cuda_test INTERFACE CUDA_TEST_HAS_CUDA=$<BOOL:${CUDA_TEST_HAS_CUDA}>)
target_include_directories(
  cuda_test
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CUDA_TEST_HAS_CUDA)
  find_package(CUDAToolkit REQUIRED)
  target_link_libraries(cuda_test INTERFACE CUDA::cudart)
endif()

if(CUDA_TEST_ENABLE_WARNINGS)
  cuda_test_apply_warnings(cuda_test)
endif()

if(TARGET GTest::gtest)
  add_library(cuda_test_testing INTERFACE)
  add_library(cuda_test::testing ALIAS cuda_test_testing)

  target_include_directories(
    cuda_test_testing
    INTERFACE
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(
    cuda_test_testing
    INTERFACE
      cuda_test::cuda_test
      $<BUILD_LOCAL_INTERFACE:GTest::gtest>
      $<INSTALL_INTERFACE:GTest::gtest>
  )

  set_target_properties(cuda_test_testing PROPERTIES EXPORT_NAME testing)
endif()

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

install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING
  PATTERN "*.hpp"
)

set(CUDA_TEST_INSTALL_TESTING_TARGET OFF)
if(TARGET cuda_test_testing)
  set(CUDA_TEST_INSTALL_TESTING_TARGET ON)
endif()

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/cuda_testConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/cmake/cuda_testConfig.cmake"
  INSTALL_DESTINATION "${CUDA_TEST_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/cmake/cuda_testConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY AnyNewerVersion
)

install(
  FILES
    "${PROJECT_BINARY_DIR}/cmake/cuda_testConfig.cmake"
    "${PROJECT_BINARY_DIR}/cmake/cuda_testConfigVersion.cmake"
  DESTINATION "${CUDA_TEST_INSTALL_CMAKEDIR}"
)

install(
  EXPORT cuda_testTargets
  FILE cuda_testTargets.cmake
  NAMESPACE cuda_test::
  DESTINATION "${CUDA_TEST_INSTALL_CMAKEDIR}"
)

install(TARGETS cuda_test EXPORT cuda_testTargets)

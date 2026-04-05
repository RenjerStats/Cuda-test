cmake_minimum_required(VERSION 3.26)

project(cuda_test VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CUDA_TEST_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/cuda_test")

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

install(
  DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
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

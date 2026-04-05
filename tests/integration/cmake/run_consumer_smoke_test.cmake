if(NOT DEFINED CMAKE_COMMAND_BIN)
  message(FATAL_ERROR "CMAKE_COMMAND_BIN is required")
endif()

if(NOT DEFINED FIXTURE_SOURCE_DIR OR NOT DEFINED FIXTURE_BINARY_DIR)
  message(FATAL_ERROR "FIXTURE_SOURCE_DIR and FIXTURE_BINARY_DIR are required")
endif()

if(NOT DEFINED TEST_KIND)
  message(FATAL_ERROR "TEST_KIND is required")
endif()

set(_config_args -S "${FIXTURE_SOURCE_DIR}" -B "${FIXTURE_BINARY_DIR}" -G "${GENERATOR_NAME}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _config_args -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _config_args -T "${GENERATOR_TOOLSET}")
endif()

if(DEFINED ENV{CTEST_CONFIGURATION_TYPE} AND NOT "$ENV{CTEST_CONFIGURATION_TYPE}" STREQUAL "")
  set(_build_config "$ENV{CTEST_CONFIGURATION_TYPE}")
elseif(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "")
  set(_build_config "${CONFIGURATION}")
else()
  set(_build_config "Debug")
endif()

file(REMOVE_RECURSE "${FIXTURE_BINARY_DIR}")
if(DEFINED INSTALL_PREFIX AND NOT INSTALL_PREFIX STREQUAL "")
  file(REMOVE_RECURSE "${INSTALL_PREFIX}")
endif()

if(TEST_KIND MATCHES "^find_package")
  execute_process(
    COMMAND "${CMAKE_COMMAND_BIN}" --install "${PROJECT_BINARY_DIR}" --prefix "${INSTALL_PREFIX}" --config "${_build_config}"
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_stdout
    ERROR_VARIABLE _install_stderr
  )
  if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "Install step failed:\n${_install_stdout}\n${_install_stderr}")
  endif()

  foreach(_required_file
          "${INSTALL_PREFIX}/include/cuda_test/cuda_test.hpp"
          "${INSTALL_PREFIX}/${INSTALL_CMAKEDIR_REL}/cuda_testConfig.cmake"
          "${INSTALL_PREFIX}/${INSTALL_CMAKEDIR_REL}/cuda_testConfigVersion.cmake")
    if(NOT EXISTS "${_required_file}")
      message(FATAL_ERROR "Expected installed file is missing: ${_required_file}")
    endif()
  endforeach()
endif()

list(APPEND _config_args "-DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}")
list(APPEND _config_args "-DCUDA_TEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}")

if(DEFINED REQUIRED_VERSION)
  list(APPEND _config_args "-DCUDA_TEST_REQUIRED_VERSION=${REQUIRED_VERSION}")
endif()

if(DEFINED EXACT_VERSION)
  list(APPEND _config_args "-DCUDA_TEST_EXACT_VERSION=${EXACT_VERSION}")
endif()

if(DEFINED ENABLE_TESTING_COMPONENT)
  list(APPEND _config_args "-DCUDA_TEST_ENABLE_TESTING_COMPONENT=${ENABLE_TESTING_COMPONENT}")
endif()

if(DEFINED GTEST_INCLUDE_DIR AND NOT GTEST_INCLUDE_DIR STREQUAL "")
  list(APPEND _config_args "-DCUDA_TEST_GTEST_INCLUDE_DIR=${GTEST_INCLUDE_DIR}")
endif()

if(DEFINED GTEST_LIBRARY AND NOT GTEST_LIBRARY STREQUAL "")
  list(APPEND _config_args "-DCUDA_TEST_GTEST_LIBRARY=${GTEST_LIBRARY}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND_BIN}" ${_config_args}
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)

if(DEFINED EXPECT_CONFIGURE_FAILURE AND EXPECT_CONFIGURE_FAILURE)
  if(_configure_result EQUAL 0)
    message(FATAL_ERROR "Configure step was expected to fail, but it succeeded")
  endif()
  return()
endif()

if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR "Configure step failed:\n${_configure_stdout}\n${_configure_stderr}")
endif()

set(_build_args --build "${FIXTURE_BINARY_DIR}" --config "${_build_config}")
execute_process(
  COMMAND "${CMAKE_COMMAND_BIN}" ${_build_args}
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_stdout
  ERROR_VARIABLE _build_stderr
)

if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR "Build step failed:\n${_build_stdout}\n${_build_stderr}")
endif()

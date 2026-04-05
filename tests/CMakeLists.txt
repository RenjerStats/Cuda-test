include(GoogleTest)

add_custom_target(cuda_test_tests
  COMMENT "Umbrella target for all tests."
)

# Helper: register a test executable under the umbrella target.
#   cuda_test_add_test(NAME <name> SOURCES <src>...)
function(cuda_test_add_test)
  cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

  add_executable(${ARG_NAME} ${ARG_SOURCES})
  target_include_directories(${ARG_NAME}
    PRIVATE
      ${PROJECT_SOURCE_DIR}
  )
  target_link_libraries(${ARG_NAME}
    PRIVATE
      cuda_test::cuda_test
      GTest::gtest_main
  )
  gtest_discover_tests(${ARG_NAME})
  add_dependencies(cuda_test_tests ${ARG_NAME})
endfunction()

function(cuda_test_add_cmake_smoke_test)
  cmake_parse_arguments(
    ARG
    "EXACT_VERSION;EXPECT_CONFIGURE_FAILURE;ENABLE_TESTING_COMPONENT"
    "NAME;FIXTURE;REQUIRED_VERSION"
    ""
    ${ARGN}
  )

  set(_fixture_binary_dir "${PROJECT_BINARY_DIR}/cmake-smoke/${ARG_NAME}")
  set(_install_prefix "${PROJECT_BINARY_DIR}/cmake-install/${ARG_NAME}")

  add_test(
    NAME ${ARG_NAME}
    COMMAND
      ${CMAKE_COMMAND}
        -DCMAKE_COMMAND_BIN=${CMAKE_COMMAND}
        -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DPROJECT_BINARY_DIR=${PROJECT_BINARY_DIR}
        -DGENERATOR_NAME=${CMAKE_GENERATOR}
        -DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
        -DCONFIGURATION=$<CONFIG>
        -DTEST_KIND=${ARG_FIXTURE}
        -DFIXTURE_SOURCE_DIR=${PROJECT_SOURCE_DIR}/tests/integration/cmake/${ARG_FIXTURE}
        -DFIXTURE_BINARY_DIR=${_fixture_binary_dir}
        -DINSTALL_PREFIX=${_install_prefix}
        -DINSTALL_CMAKEDIR_REL=${CUDA_TEST_INSTALL_CMAKEDIR}
        -DREQUIRED_VERSION=${ARG_REQUIRED_VERSION}
        -DEXACT_VERSION=$<BOOL:${ARG_EXACT_VERSION}>
        -DEXPECT_CONFIGURE_FAILURE=$<BOOL:${ARG_EXPECT_CONFIGURE_FAILURE}>
        -DENABLE_TESTING_COMPONENT=$<BOOL:${ARG_ENABLE_TESTING_COMPONENT}>
        -DGTEST_INCLUDE_DIR=${PROJECT_BINARY_DIR}/_deps/googletest-src/googletest/include
        -DGTEST_LIBRARY=$<TARGET_FILE:GTest::gtest>
        -P ${PROJECT_SOURCE_DIR}/tests/integration/cmake/run_consumer_smoke_test.cmake
  )
endfunction()

# ---- Unit tests ----
cuda_test_add_test(NAME analysis_advisor_test SOURCES unit/analysis/advisor_test.cpp)
cuda_test_add_test(NAME analysis_metrics_test SOURCES unit/analysis/metrics_test.cpp)
cuda_test_add_test(NAME analysis_bandwidth_test SOURCES unit/analysis/bandwidth_test.cpp)
cuda_test_add_test(NAME analysis_metrics_extended_test SOURCES unit/analysis/metrics_extended_test.cpp)
cuda_test_add_test(NAME core_types_test SOURCES unit/core/core_types_test.cpp)
cuda_test_add_test(NAME autotune_search_test SOURCES unit/autotune/autotune_search_test.cpp)
cuda_test_add_test(NAME benchmark_stats_test SOURCES unit/benchmark/benchmark_stats_test.cpp)
cuda_test_add_test(NAME benchmark_runner_test SOURCES unit/benchmark/benchmark_runner_test.cpp)
cuda_test_add_test(NAME pipeline_kernel_descriptor_test SOURCES unit/pipeline/kernel_descriptor_test.cpp)
cuda_test_add_test(NAME pipeline_suite_test SOURCES unit/pipeline/pipeline_test.cpp)
cuda_test_add_test(NAME reporting_export_test SOURCES unit/reporting/export_test.cpp)
cuda_test_add_test(NAME testing_validation_test SOURCES unit/testing/validation_test.cpp)

if(CUDA_TEST_HAS_CUDA)
  cuda_test_add_test(NAME analysis_cuda_runtime_test SOURCES integration/analysis/analysis_cuda_runtime_test.cu)
  cuda_test_add_test(NAME core_cuda_runtime_test SOURCES unit/core/core_cuda_runtime_test.cpp)
  cuda_test_add_test(NAME kernel_integration_test SOURCES integration/kernel_integration_test.cu)
  cuda_test_add_test(NAME pipeline_kernel_descriptor_cuda_test SOURCES integration/pipeline/kernel_descriptor_cuda_test.cu)
  cuda_test_add_test(NAME pipeline_diagnose_cuda_test SOURCES integration/pipeline/pipeline_diagnose_test.cu)
  cuda_test_add_test(NAME pipeline_suite_cuda_test SOURCES integration/pipeline/pipeline_cuda_test.cu)
  cuda_test_add_test(NAME profiling_staged_timer_test SOURCES unit/profiling/staged_timer_test.cu)
  cuda_test_add_test(NAME testing_kernel_fixture_test SOURCES unit/testing/kernel_test_fixture_test.cu)
endif()

# ---- Integration tests ----
cuda_test_add_cmake_smoke_test(
  NAME cmake_find_package_exact_test
  FIXTURE find_package_test
  REQUIRED_VERSION 0.2.0
  EXACT_VERSION
)
cuda_test_add_cmake_smoke_test(
  NAME cmake_find_package_compatible_test
  FIXTURE find_package_test
  REQUIRED_VERSION 0.1.0
)
cuda_test_add_cmake_smoke_test(
  NAME cmake_find_package_incompatible_test
  FIXTURE find_package_test
  REQUIRED_VERSION 999.0
  EXPECT_CONFIGURE_FAILURE
)
cuda_test_add_cmake_smoke_test(
  NAME cmake_find_package_testing_component_test
  FIXTURE find_package_test
  REQUIRED_VERSION 0.2.0
  EXACT_VERSION
  ENABLE_TESTING_COMPONENT
)
cuda_test_add_cmake_smoke_test(
  NAME cmake_fetch_content_test
  FIXTURE fetch_content_test
  REQUIRED_VERSION 0.2.0
)
cuda_test_add_cmake_smoke_test(
  NAME cmake_subdirectory_basic_test
  FIXTURE subdirectory_test
  REQUIRED_VERSION 0.2.0
)
cuda_test_add_cmake_smoke_test(
  NAME cmake_subdirectory_testing_target_test
  FIXTURE subdirectory_test
  REQUIRED_VERSION 0.2.0
  ENABLE_TESTING_COMPONENT
)

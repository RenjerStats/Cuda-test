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

# ---- Unit tests ----
cuda_test_add_test(NAME core_types_test SOURCES unit/core/core_types_test.cpp)
cuda_test_add_test(NAME benchmark_stats_test SOURCES unit/benchmark/benchmark_stats_test.cpp)
cuda_test_add_test(NAME benchmark_runner_test SOURCES unit/benchmark/benchmark_runner_test.cpp)
cuda_test_add_test(NAME testing_validation_test SOURCES unit/testing/validation_test.cpp)

if(CUDA_TEST_HAS_CUDA)
  cuda_test_add_test(NAME core_cuda_runtime_test SOURCES unit/core/core_cuda_runtime_test.cpp)
  cuda_test_add_test(NAME profiling_staged_timer_test SOURCES unit/profiling/staged_timer_test.cu)
  cuda_test_add_test(NAME testing_kernel_fixture_test SOURCES unit/testing/kernel_test_fixture_test.cu)
endif()

# ---- Integration tests ----
# cuda_test_add_test(NAME integration_smoke SOURCES integration/smoke_test.cpp)

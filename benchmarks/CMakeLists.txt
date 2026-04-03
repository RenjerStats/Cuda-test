add_custom_target(cuda_test_benchmarks
  COMMENT "Umbrella target for all benchmarks."
)

# Helper: register a benchmark executable under the umbrella target.
#   cuda_test_add_benchmark(NAME <name> SOURCES <src>...)
function(cuda_test_add_benchmark)
  cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

  add_executable(${ARG_NAME} ${ARG_SOURCES})
  target_link_libraries(${ARG_NAME}
    PRIVATE
      cuda_test::cuda_test
      benchmark::benchmark
      benchmark::benchmark_main
  )
  add_dependencies(cuda_test_benchmarks ${ARG_NAME})
endfunction()

cuda_test_add_benchmark(NAME benchmark_runner_smoke SOURCES benchmark_runner_smoke.cpp)

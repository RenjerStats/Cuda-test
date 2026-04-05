add_custom_target(cuda_test_examples
  COMMENT "Umbrella target for all examples."
)

function(cuda_test_add_example)
  cmake_parse_arguments(ARG "" "NAME" "SOURCES" ${ARGN})

  add_executable(${ARG_NAME} ${ARG_SOURCES})
  target_link_libraries(${ARG_NAME}
    PRIVATE
      cuda_test::cuda_test
  )
  add_dependencies(cuda_test_examples ${ARG_NAME})
endfunction()

if(CUDA_TEST_HAS_CUDA)
  cuda_test_add_example(
    NAME full_workflow_demo
    SOURCES full_workflow_demo.cu
  )
endif()

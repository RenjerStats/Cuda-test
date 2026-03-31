add_library(cuda_test INTERFACE)
add_library(cuda_test::cuda_test ALIAS cuda_test)

target_compile_features(cuda_test INTERFACE cxx_std_17)
target_include_directories(
  cuda_test
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CUDA_TEST_ENABLE_WARNINGS)
  cuda_test_apply_warnings(cuda_test)
endif()

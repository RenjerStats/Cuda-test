#include <cuda_test/cuda_test.hpp>

#if defined(CUDA_TEST_USE_TESTING_COMPONENT)
#include <cuda_test/testing/validation.hpp>
#endif

int main() {
    cuda_test::core::KernelLaunchConfig config;
    config.block = dim3(128, 1, 1);
    config.grid = dim3(8, 1, 1);
    const bool version_ok = cuda_test::detail::version_major == 0 &&
                            cuda_test::detail::version_minor == 2 &&
                            cuda_test::detail::version_patch == 0;

#if defined(CUDA_TEST_USE_TESTING_COMPONENT)
    cuda_test::testing::detail::validate_array_inputs<int>(nullptr, nullptr, 0);
#endif

    return version_ok && config.block.x == 128U && config.grid.x == 8U ? 0 : 1;
}

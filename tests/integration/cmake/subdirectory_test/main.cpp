#include <cuda_test/cuda_test.hpp>

#if defined(CUDA_TEST_USE_TESTING_COMPONENT)
#include <cuda_test/testing/validation.hpp>
#endif

int main() {
    cuda_test::core::KernelLaunchConfig config;
    config.device_id = 3;

#if defined(CUDA_TEST_USE_TESTING_COMPONENT)
    cuda_test::testing::detail::validate_array_inputs<int>(nullptr, nullptr, 0);
#endif

    return config.device_id == 3 && cuda_test::detail::version_minor == 2 ? 0 : 1;
}

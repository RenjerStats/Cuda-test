#include <cuda_test/cuda_test.hpp>

int main() {
    cuda_test::core::KernelLaunchConfig config;
    config.shared_mem = 64;

    return config.shared_mem == 64U && cuda_test::detail::version_minor == 2 ? 0 : 1;
}

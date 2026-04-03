#pragma once

#include "cuda_test/autotune/search.hpp"
#include "cuda_test/core/device_memory.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/profiling/staged_timer.hpp"
#include "tests/fixtures/kernel_suite_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cuda_test::examples::kernels {

inline constexpr float float_tolerance = 1e-5f;

inline core::KernelLaunchConfig make_launch_config(std::size_t problem_size,
                                                   unsigned int block_size,
                                                   int device_id = 0,
                                                   std::size_t shared_mem = 0,
                                                   unsigned int grid_wave_multiplier = 1) {
    if (block_size == 0U) {
        throw std::invalid_argument("Block size must be greater than zero");
    }
    if (grid_wave_multiplier == 0U) {
        throw std::invalid_argument("Grid wave multiplier must be greater than zero");
    }

    const unsigned int base_grid =
        problem_size == 0U ? 0U
                           : static_cast<unsigned int>((problem_size + block_size - 1U) / block_size);

    core::KernelLaunchConfig config;
    config.grid = dim3(base_grid * grid_wave_multiplier, 1, 1);
    config.block = dim3(block_size, 1, 1);
    config.shared_mem = shared_mem;
    config.device_id = device_id;
    return config;
}

inline autotune::AutoTuneSpec make_default_autotune_spec(int device_id = 0) {
    autotune::AutoTuneSpec spec;
    spec.block_sizes = {64, 128, 256};
    spec.grid_wave_multipliers = {1, 2};
    spec.warmup_runs = 5;
    spec.measure_runs = 30;
    spec.device_id = device_id;
    return spec;
}

inline bool launch_covers_problem(const core::KernelLaunchConfig& config, std::size_t problem_size) noexcept {
    return static_cast<std::size_t>(config.grid.x) * static_cast<std::size_t>(config.block.x) >= problem_size;
}

inline bool floats_match(const std::vector<float>& actual,
                         const std::vector<float>& expected,
                         float eps = float_tolerance) {
    if (actual.size() != expected.size()) {
        return false;
    }

    for (std::size_t index = 0; index < actual.size(); ++index) {
        if (std::fabs(actual[index] - expected[index]) > eps) {
            return false;
        }
    }

    return true;
}

inline bool ints_match(const std::vector<int>& actual, const std::vector<int>& expected) {
    return actual == expected;
}

inline void select_device_for_config(const core::KernelLaunchConfig& config) {
    CUDA_CHECK(cudaSetDevice(config.device_id));
}

static __global__ void density_update_kernel(const float* density,
                                             const float* delta,
                                             float* output,
                                             std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = density[index] + delta[index];
    }
}

static __global__ void physics_integration_kernel(const float* position,
                                                  const float* velocity,
                                                  float dt,
                                                  float* output,
                                                  std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = position[index] + velocity[index] * dt;
    }
}

static __global__ void contact_flag_kernel(const float* lhs,
                                           const float* rhs,
                                           float threshold,
                                           int* output,
                                           std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = (fabsf(lhs[index] - rhs[index]) <= threshold) ? 1 : 0;
    }
}

static __global__ void active_compaction_kernel(const int* active_mask,
                                                int* compacted_indices,
                                                int* active_count,
                                                std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size && active_mask[index] != 0) {
        const int slot = atomicAdd(active_count, 1);
        compacted_indices[slot] = static_cast<int>(index);
    }
}

static __global__ void buffer_generation_kernel(const int* source,
                                                int multiplier,
                                                int offset,
                                                int* output,
                                                std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = source[index] * multiplier + offset;
    }
}

static __global__ void interval_intersection_kernel(const float* min_values,
                                                    const float* max_values,
                                                    float query,
                                                    int* output,
                                                    std::size_t size) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index < size) {
        output[index] = (query >= min_values[index] && query <= max_values[index]) ? 1 : 0;
    }
}

class DensityUpdateCase {
public:
    explicit DensityUpdateCase(fixtures::DensityUpdateFixture fixture)
        : fixture_(std::move(fixture)),
          device_density_(fixture_.size()),
          device_delta_(fixture_.size()),
          device_output_(fixture_.size()) {
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return fixture_.size();
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return make_launch_config(problem_size(), 128U, device_id);
    }

    bool validate(const core::KernelLaunchConfig& config) {
        const std::vector<float> actual = execute(config);
        return floats_match(actual, fixture_.expected);
    }

    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        profiling::StagedTimer timer;
        timer.start_total();
        timer.start_h2d();
        device_density_.copy_from_host(fixture_.density);
        device_delta_.copy_from_host(fixture_.delta);
        timer.stop_h2d();

        timer.start_kernel();
        density_update_kernel<<<config.grid, config.block>>>(
            device_density_.data(), device_delta_.data(), device_output_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        (void)device_output_.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();
        return timer.breakdown();
    }

private:
    std::vector<float> execute(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        device_density_.copy_from_host(fixture_.density);
        device_delta_.copy_from_host(fixture_.delta);
        density_update_kernel<<<config.grid, config.block>>>(
            device_density_.data(), device_delta_.data(), device_output_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        return device_output_.copy_to_host();
    }

    std::string name_ = "density_update";
    fixtures::DensityUpdateFixture fixture_;
    core::DeviceMemory<float> device_density_;
    core::DeviceMemory<float> device_delta_;
    core::DeviceMemory<float> device_output_;
};

class PhysicsIntegrationCase {
public:
    explicit PhysicsIntegrationCase(fixtures::PhysicsIntegrationFixture fixture)
        : fixture_(std::move(fixture)),
          device_position_(fixture_.size()),
          device_velocity_(fixture_.size()),
          device_output_(fixture_.size()) {
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return fixture_.size();
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return make_launch_config(problem_size(), 128U, device_id);
    }

    bool validate(const core::KernelLaunchConfig& config) {
        const std::vector<float> actual = execute(config);
        return floats_match(actual, fixture_.expected);
    }

    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        profiling::StagedTimer timer;
        timer.start_total();
        timer.start_h2d();
        device_position_.copy_from_host(fixture_.position);
        device_velocity_.copy_from_host(fixture_.velocity);
        timer.stop_h2d();

        timer.start_kernel();
        physics_integration_kernel<<<config.grid, config.block>>>(
            device_position_.data(),
            device_velocity_.data(),
            fixture_.dt,
            device_output_.data(),
            problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        (void)device_output_.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();
        return timer.breakdown();
    }

private:
    std::vector<float> execute(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        device_position_.copy_from_host(fixture_.position);
        device_velocity_.copy_from_host(fixture_.velocity);
        physics_integration_kernel<<<config.grid, config.block>>>(
            device_position_.data(),
            device_velocity_.data(),
            fixture_.dt,
            device_output_.data(),
            problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        return device_output_.copy_to_host();
    }

    std::string name_ = "physics_integration";
    fixtures::PhysicsIntegrationFixture fixture_;
    core::DeviceMemory<float> device_position_;
    core::DeviceMemory<float> device_velocity_;
    core::DeviceMemory<float> device_output_;
};

class ContactFlagCase {
public:
    explicit ContactFlagCase(fixtures::ContactFlagFixture fixture)
        : fixture_(std::move(fixture)),
          device_lhs_(fixture_.size()),
          device_rhs_(fixture_.size()),
          device_output_(fixture_.size()) {
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return fixture_.size();
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return make_launch_config(problem_size(), 128U, device_id);
    }

    bool validate(const core::KernelLaunchConfig& config) {
        const std::vector<int> actual = execute(config);
        return ints_match(actual, fixture_.expected);
    }

    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        profiling::StagedTimer timer;
        timer.start_total();
        timer.start_h2d();
        device_lhs_.copy_from_host(fixture_.lhs);
        device_rhs_.copy_from_host(fixture_.rhs);
        timer.stop_h2d();

        timer.start_kernel();
        contact_flag_kernel<<<config.grid, config.block>>>(
            device_lhs_.data(), device_rhs_.data(), fixture_.threshold, device_output_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        (void)device_output_.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();
        return timer.breakdown();
    }

private:
    std::vector<int> execute(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        device_lhs_.copy_from_host(fixture_.lhs);
        device_rhs_.copy_from_host(fixture_.rhs);
        contact_flag_kernel<<<config.grid, config.block>>>(
            device_lhs_.data(), device_rhs_.data(), fixture_.threshold, device_output_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        return device_output_.copy_to_host();
    }

    std::string name_ = "contact_flag";
    fixtures::ContactFlagFixture fixture_;
    core::DeviceMemory<float> device_lhs_;
    core::DeviceMemory<float> device_rhs_;
    core::DeviceMemory<int> device_output_;
};

class ActiveCompactionCase {
public:
    explicit ActiveCompactionCase(fixtures::ActiveCompactionFixture fixture)
        : fixture_(std::move(fixture)),
          device_mask_(fixture_.size()),
          device_output_indices_(fixture_.size()),
          device_active_count_(1) {
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return fixture_.size();
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return make_launch_config(problem_size(), 128U, device_id);
    }

    bool validate(const core::KernelLaunchConfig& config) {
        const auto [indices, count] = execute(config);
        std::vector<int> trimmed(indices.begin(), indices.begin() + count);
        std::sort(trimmed.begin(), trimmed.end());

        std::vector<int> expected = fixture_.expected_indices;
        std::sort(expected.begin(), expected.end());
        return count == fixture_.expected_count && ints_match(trimmed, expected);
    }

    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        profiling::StagedTimer timer;
        const std::vector<int> zero_count(1, 0);
        timer.start_total();
        timer.start_h2d();
        device_mask_.copy_from_host(fixture_.active_mask);
        device_active_count_.copy_from_host(zero_count);
        timer.stop_h2d();

        timer.start_kernel();
        active_compaction_kernel<<<config.grid, config.block>>>(
            device_mask_.data(), device_output_indices_.data(), device_active_count_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        (void)device_output_indices_.copy_to_host();
        (void)device_active_count_.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();
        return timer.breakdown();
    }

private:
    std::pair<std::vector<int>, int> execute(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        const std::vector<int> zero_count(1, 0);
        device_mask_.copy_from_host(fixture_.active_mask);
        device_active_count_.copy_from_host(zero_count);
        active_compaction_kernel<<<config.grid, config.block>>>(
            device_mask_.data(), device_output_indices_.data(), device_active_count_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());

        const std::vector<int> indices = device_output_indices_.copy_to_host();
        const std::vector<int> count = device_active_count_.copy_to_host();
        return {indices, count.front()};
    }

    std::string name_ = "active_compaction";
    fixtures::ActiveCompactionFixture fixture_;
    core::DeviceMemory<int> device_mask_;
    core::DeviceMemory<int> device_output_indices_;
    core::DeviceMemory<int> device_active_count_;
};

class BufferGenerationCase {
public:
    explicit BufferGenerationCase(fixtures::BufferGenerationFixture fixture)
        : fixture_(std::move(fixture)),
          device_source_(fixture_.size()),
          device_output_(fixture_.size()) {
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return fixture_.size();
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return make_launch_config(problem_size(), 128U, device_id);
    }

    bool validate(const core::KernelLaunchConfig& config) {
        const std::vector<int> actual = execute(config);
        return ints_match(actual, fixture_.expected);
    }

    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        profiling::StagedTimer timer;
        timer.start_total();
        timer.start_h2d();
        device_source_.copy_from_host(fixture_.source);
        timer.stop_h2d();

        timer.start_kernel();
        buffer_generation_kernel<<<config.grid, config.block>>>(
            device_source_.data(),
            fixture_.multiplier,
            fixture_.offset,
            device_output_.data(),
            problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        (void)device_output_.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();
        return timer.breakdown();
    }

private:
    std::vector<int> execute(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        device_source_.copy_from_host(fixture_.source);
        buffer_generation_kernel<<<config.grid, config.block>>>(
            device_source_.data(),
            fixture_.multiplier,
            fixture_.offset,
            device_output_.data(),
            problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        return device_output_.copy_to_host();
    }

    std::string name_ = "buffer_generation";
    fixtures::BufferGenerationFixture fixture_;
    core::DeviceMemory<int> device_source_;
    core::DeviceMemory<int> device_output_;
};

class IntervalIntersectionCase {
public:
    explicit IntervalIntersectionCase(fixtures::IntersectionFixture fixture)
        : fixture_(std::move(fixture)),
          device_min_(fixture_.size()),
          device_max_(fixture_.size()),
          device_output_(fixture_.size()) {
    }

    [[nodiscard]] const std::string& name() const noexcept {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return fixture_.size();
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return make_launch_config(problem_size(), 128U, device_id);
    }

    bool validate(const core::KernelLaunchConfig& config) {
        const std::vector<int> actual = execute(config);
        return ints_match(actual, fixture_.expected);
    }

    core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        profiling::StagedTimer timer;
        timer.start_total();
        timer.start_h2d();
        device_min_.copy_from_host(fixture_.min_values);
        device_max_.copy_from_host(fixture_.max_values);
        timer.stop_h2d();

        timer.start_kernel();
        interval_intersection_kernel<<<config.grid, config.block>>>(
            device_min_.data(), device_max_.data(), fixture_.query, device_output_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        (void)device_output_.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();
        return timer.breakdown();
    }

private:
    std::vector<int> execute(const core::KernelLaunchConfig& config) {
        select_device_for_config(config);

        device_min_.copy_from_host(fixture_.min_values);
        device_max_.copy_from_host(fixture_.max_values);
        interval_intersection_kernel<<<config.grid, config.block>>>(
            device_min_.data(), device_max_.data(), fixture_.query, device_output_.data(), problem_size());
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        return device_output_.copy_to_host();
    }

    std::string name_ = "interval_intersection";
    fixtures::IntersectionFixture fixture_;
    core::DeviceMemory<float> device_min_;
    core::DeviceMemory<float> device_max_;
    core::DeviceMemory<int> device_output_;
};

} // namespace cuda_test::examples::kernels

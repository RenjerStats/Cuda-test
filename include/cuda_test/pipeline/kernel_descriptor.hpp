#pragma once

#include "cuda_test/core/device_info.hpp"
#include "cuda_test/core/device_memory.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/core/types.hpp"
#include "cuda_test/profiling/staged_timer.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuda_test::pipeline {

template <typename InputFactory, typename ExpectedFactory, typename LaunchFn>
class KernelDescriptorBuilder;

namespace detail {

struct unset_t {
};

template <typename T>
struct is_std_vector : std::false_type {
};

template <typename T, typename Allocator>
struct is_std_vector<std::vector<T, Allocator>> : std::true_type {
    using value_type = T;
};

template <typename T>
inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

template <typename T>
struct is_tuple_of_vectors : std::false_type {
};

template <typename... Ts>
struct is_tuple_of_vectors<std::tuple<Ts...>> : std::bool_constant<(is_std_vector_v<Ts> && ...)> {
};

template <typename T>
inline constexpr bool is_tuple_of_vectors_v = is_tuple_of_vectors<T>::value;

template <typename Tuple>
struct device_buffers_tuple;

template <typename... Vectors>
struct device_buffers_tuple<std::tuple<Vectors...>> {
    using type = std::tuple<core::DeviceMemory<typename Vectors::value_type>...>;
};

template <typename Tuple>
using device_buffers_tuple_t = typename device_buffers_tuple<Tuple>::type;

template <typename Tuple>
struct device_buffer_refs_tuple;

template <typename... Vectors>
struct device_buffer_refs_tuple<std::tuple<Vectors...>> {
    using type = std::tuple<core::DeviceMemory<typename Vectors::value_type>&...>;
};

template <typename Tuple>
using device_buffer_refs_tuple_t = typename device_buffer_refs_tuple<Tuple>::type;

template <typename Tuple, std::size_t... Indexes>
inline void validate_input_sizes_impl(const Tuple& inputs,
                                      std::size_t problem_size,
                                      std::index_sequence<Indexes...>) {
    const bool all_match = ((std::get<Indexes>(inputs).size() == problem_size) && ...);
    if (!all_match) {
        throw std::invalid_argument("All input vectors must match KernelDescriptor problem_size");
    }
}

template <typename Tuple>
inline void validate_input_sizes(const Tuple& inputs, std::size_t problem_size) {
    validate_input_sizes_impl(
        inputs, problem_size, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
}

template <typename ExpectedVector>
inline void validate_expected_size(const ExpectedVector& expected, std::size_t problem_size) {
    if (expected.size() != problem_size) {
        throw std::invalid_argument("Expected output vector must match KernelDescriptor problem_size");
    }
}

template <typename Tuple, std::size_t... Indexes>
inline auto make_device_buffers_impl(const Tuple& host_inputs, std::index_sequence<Indexes...>) {
    return device_buffers_tuple_t<Tuple>(
        core::DeviceMemory<typename std::tuple_element_t<Indexes, Tuple>::value_type>(
            std::get<Indexes>(host_inputs).size())...);
}

template <typename Tuple>
inline auto make_device_buffers(const Tuple& host_inputs) {
    return make_device_buffers_impl(
        host_inputs, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
}

template <typename HostTuple, typename DeviceTuple, std::size_t... Indexes>
inline void copy_inputs_to_device_impl(const HostTuple& host_inputs,
                                       DeviceTuple& device_inputs,
                                       std::index_sequence<Indexes...>) {
    (std::get<Indexes>(device_inputs).copy_from_host(std::get<Indexes>(host_inputs)), ...);
}

template <typename HostTuple, typename DeviceTuple>
inline void copy_inputs_to_device(const HostTuple& host_inputs, DeviceTuple& device_inputs) {
    copy_inputs_to_device_impl(
        host_inputs, device_inputs, std::make_index_sequence<std::tuple_size<HostTuple>::value>{});
}

template <typename DeviceTuple, std::size_t... Indexes>
inline auto tie_device_buffers_impl(DeviceTuple& device_inputs, std::index_sequence<Indexes...>) {
    return std::forward_as_tuple(std::get<Indexes>(device_inputs)...);
}

template <typename DeviceTuple>
inline auto tie_device_buffers(DeviceTuple& device_inputs) {
    using BareTuple = std::remove_reference_t<DeviceTuple>;
    return tie_device_buffers_impl(
        device_inputs, std::make_index_sequence<std::tuple_size<BareTuple>::value>{});
}

template <typename T>
inline bool values_match(const T& actual, const T& expected, const std::optional<double>& tolerance) {
    if (!tolerance.has_value()) {
        return actual == expected;
    }

    if constexpr (std::is_arithmetic_v<T>) {
        return std::fabs(static_cast<double>(actual) - static_cast<double>(expected)) <= *tolerance;
    } else {
        throw std::invalid_argument("Tolerance comparison requires arithmetic output values");
    }
}

template <typename T>
inline bool vectors_match(const std::vector<T>& actual,
                          const std::vector<T>& expected,
                          const std::optional<double>& tolerance) {
    if (actual.size() != expected.size()) {
        return false;
    }

    for (std::size_t index = 0; index < actual.size(); ++index) {
        if (!values_match(actual[index], expected[index], tolerance)) {
            return false;
        }
    }

    return true;
}

inline void validate_device_id(int device_id) {
    const int visible_devices = core::device_count();
    if (visible_devices > 0 && !core::device_exists(device_id)) {
        throw std::out_of_range("Requested CUDA device does not exist");
    }
}

inline void select_device_for_config(const core::KernelLaunchConfig& config) {
    validate_device_id(config.device_id);

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
    CUDA_CHECK(cudaSetDevice(config.device_id));
#else
    (void)config;
    throw std::runtime_error("CUDA support is disabled");
#endif
}

inline core::KernelLaunchConfig make_baseline_config(std::size_t problem_size, int device_id) {
    validate_device_id(device_id);

    core::KernelLaunchConfig config;
    config.grid =
        dim3(problem_size == 0U ? 0U
                                : static_cast<unsigned int>((problem_size + 127U) / 128U),
             1,
             1);
    config.block = dim3(128U, 1, 1);
    config.shared_mem = 0U;
    config.device_id = device_id;
    return config;
}

template <typename InputFactory, typename ExpectedFactory, typename LaunchFn>
class KernelDescriptorModel;

} // namespace detail

class KernelDescriptor {
public:
    KernelDescriptor() = default;

    // Copyable handle over shared descriptor state. Concurrent validate()/measure() calls on
    // shared instances are not thread-safe because factories may hold mutable callable state.
    [[nodiscard]] const std::string& name() const noexcept {
        static const std::string empty_name;
        return impl_ == nullptr ? empty_name : impl_->name();
    }

    [[nodiscard]] std::size_t problem_size() const noexcept {
        return impl_ == nullptr ? 0U : impl_->problem_size();
    }

    [[nodiscard]] bool validate(const core::KernelLaunchConfig& config) const {
        return implementation().validate(config);
    }

    [[nodiscard]] core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) const {
        return implementation().measure(config);
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id = 0) const {
        return implementation().baseline_config(device_id);
    }

private:
    class Concept {
    public:
        virtual ~Concept() = default;

        [[nodiscard]] virtual const std::string& name() const noexcept = 0;
        [[nodiscard]] virtual std::size_t problem_size() const noexcept = 0;
        [[nodiscard]] virtual bool validate(const core::KernelLaunchConfig& config) const = 0;
        [[nodiscard]] virtual core::ProfilingBreakdown measure(
            const core::KernelLaunchConfig& config) const = 0;
        [[nodiscard]] virtual core::KernelLaunchConfig baseline_config(int device_id) const = 0;
    };

    explicit KernelDescriptor(std::shared_ptr<const Concept> impl) : impl_(std::move(impl)) {
    }

    [[nodiscard]] const Concept& implementation() const {
        if (impl_ == nullptr) {
            throw std::logic_error("KernelDescriptor is empty");
        }

        return *impl_;
    }

    std::shared_ptr<const Concept> impl_{};

    template <typename InputFactory, typename ExpectedFactory, typename LaunchFn>
    friend class KernelDescriptorBuilder;
    template <typename InputFactory, typename ExpectedFactory, typename LaunchFn>
    friend class detail::KernelDescriptorModel;
};

namespace detail {

template <typename InputFactory, typename ExpectedFactory, typename LaunchFn>
class KernelDescriptorModel final : public KernelDescriptor::Concept {
public:
    using InputTuple = std::decay_t<std::invoke_result_t<InputFactory&>>;
    using ExpectedVector = std::decay_t<std::invoke_result_t<ExpectedFactory&>>;
    using OutputValue = typename ExpectedVector::value_type;
    using DeviceBuffers = device_buffers_tuple_t<InputTuple>;
    using DeviceInputRefs = device_buffer_refs_tuple_t<InputTuple>;

    struct RunArtifacts {
        core::ProfilingBreakdown breakdown{};
        std::vector<OutputValue> output;
    };

    KernelDescriptorModel(std::string name,
                          std::size_t problem_size,
                          InputFactory input_factory,
                          ExpectedFactory expected_factory,
                          std::optional<double> tolerance,
                          LaunchFn launch_fn)
        : name_(std::move(name)),
          problem_size_(problem_size),
          input_factory_(std::move(input_factory)),
          expected_factory_(std::move(expected_factory)),
          tolerance_(tolerance),
          launch_fn_(std::move(launch_fn)) {
    }

    [[nodiscard]] const std::string& name() const noexcept override {
        return name_;
    }

    [[nodiscard]] std::size_t problem_size() const noexcept override {
        return problem_size_;
    }

    [[nodiscard]] bool validate(const core::KernelLaunchConfig& config) const override {
        const InputTuple host_inputs = std::invoke(input_factory_);
        validate_input_sizes(host_inputs, problem_size_);

        const ExpectedVector expected = std::invoke(expected_factory_);
        validate_expected_size(expected, problem_size_);

        if (problem_size_ == 0U) {
            return vectors_match(std::vector<OutputValue>{}, expected, tolerance_);
        }

        return vectors_match(run_once(config, host_inputs).output, expected, tolerance_);
    }

    [[nodiscard]] core::ProfilingBreakdown measure(const core::KernelLaunchConfig& config) const override {
        const InputTuple host_inputs = std::invoke(input_factory_);
        validate_input_sizes(host_inputs, problem_size_);

        if (problem_size_ == 0U) {
            return {};
        }

        return run_once(config, host_inputs).breakdown;
    }

    [[nodiscard]] core::KernelLaunchConfig baseline_config(int device_id) const override {
        return make_baseline_config(problem_size_, device_id);
    }

private:
    [[nodiscard]] RunArtifacts run_once(const core::KernelLaunchConfig& config,
                                        const InputTuple& host_inputs) const {
        select_device_for_config(config);

        DeviceBuffers device_inputs = make_device_buffers(host_inputs);
        core::DeviceMemory<OutputValue> output_device(problem_size_);

        profiling::StagedTimer timer;
        timer.start_total();
        timer.start_h2d();
        copy_inputs_to_device(host_inputs, device_inputs);
        timer.stop_h2d();

        DeviceInputRefs device_input_refs = tie_device_buffers(device_inputs);
        timer.start_kernel();
        std::invoke(launch_fn_, config, device_input_refs, output_device);
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
        timer.stop_kernel();

        timer.start_d2h();
        std::vector<OutputValue> host_output = output_device.copy_to_host();
        timer.stop_d2h();
        timer.stop_total();

        RunArtifacts artifacts;
        artifacts.breakdown = timer.breakdown();
        artifacts.output = std::move(host_output);
        return artifacts;
#else
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    std::string name_;
    std::size_t problem_size_ = 0U;
    mutable InputFactory input_factory_;
    mutable ExpectedFactory expected_factory_;
    std::optional<double> tolerance_{};
    mutable LaunchFn launch_fn_;
};

} // namespace detail

template <typename InputFactory = detail::unset_t,
          typename ExpectedFactory = detail::unset_t,
          typename LaunchFn = detail::unset_t>
class KernelDescriptorBuilder {
public:
    explicit KernelDescriptorBuilder(std::string name) : name_(std::move(name)) {
    }

    [[nodiscard]] auto problem_size(std::size_t problem_size) const {
        return KernelDescriptorBuilder(
            name_, std::optional<std::size_t>(problem_size), inputs_, expected_, tolerance_, launch_);
    }

    template <typename NewInputFactory>
    [[nodiscard]] auto inputs(NewInputFactory&& input_factory) const {
        using StoredInputFactory = std::decay_t<NewInputFactory>;
        static_assert(std::is_invocable_v<StoredInputFactory&>,
                      "KernelDescriptor inputs factory must be callable with no arguments");
        using InputTuple = std::decay_t<std::invoke_result_t<StoredInputFactory&>>;
        static_assert(detail::is_tuple_of_vectors_v<InputTuple>,
                      "KernelDescriptor inputs factory must return std::tuple<std::vector<T>...>");

        return KernelDescriptorBuilder<StoredInputFactory, ExpectedFactory, LaunchFn>(
            name_,
            problem_size_,
            std::optional<StoredInputFactory>(std::forward<NewInputFactory>(input_factory)),
            expected_,
            tolerance_,
            launch_);
    }

    template <typename NewExpectedFactory>
    [[nodiscard]] auto expected(NewExpectedFactory&& expected_factory) const {
        using StoredExpectedFactory = std::decay_t<NewExpectedFactory>;
        static_assert(std::is_invocable_v<StoredExpectedFactory&>,
                      "KernelDescriptor expected factory must be callable with no arguments");
        using ExpectedVector = std::decay_t<std::invoke_result_t<StoredExpectedFactory&>>;
        static_assert(detail::is_std_vector_v<ExpectedVector>,
                      "KernelDescriptor expected factory must return std::vector<T>");

        return KernelDescriptorBuilder<InputFactory, StoredExpectedFactory, LaunchFn>(
            name_,
            problem_size_,
            inputs_,
            std::optional<StoredExpectedFactory>(std::forward<NewExpectedFactory>(expected_factory)),
            tolerance_,
            launch_);
    }

    [[nodiscard]] auto tolerance(double tolerance) const {
        return KernelDescriptorBuilder(
            name_, problem_size_, inputs_, expected_, std::optional<double>(tolerance), launch_);
    }

    template <typename NewLaunchFn>
    [[nodiscard]] auto launch(NewLaunchFn&& launch_fn) const {
        using StoredLaunchFn = std::decay_t<NewLaunchFn>;

        return KernelDescriptorBuilder<InputFactory, ExpectedFactory, StoredLaunchFn>(
            name_,
            problem_size_,
            inputs_,
            expected_,
            tolerance_,
            std::optional<StoredLaunchFn>(std::forward<NewLaunchFn>(launch_fn)));
    }

    [[nodiscard]] KernelDescriptor build() const {
        if (!problem_size_.has_value()) {
            throw std::invalid_argument("KernelDescriptor requires problem_size");
        }

        if constexpr (std::is_same_v<InputFactory, detail::unset_t>) {
            throw std::invalid_argument("KernelDescriptor requires inputs factory");
        } else if constexpr (std::is_same_v<ExpectedFactory, detail::unset_t>) {
            throw std::invalid_argument("KernelDescriptor requires expected factory");
        } else if constexpr (std::is_same_v<LaunchFn, detail::unset_t>) {
            throw std::invalid_argument("KernelDescriptor requires launch callable");
        } else {
            if (!inputs_.has_value()) {
                throw std::invalid_argument("KernelDescriptor requires inputs factory");
            }
            if (!expected_.has_value()) {
                throw std::invalid_argument("KernelDescriptor requires expected factory");
            }
            if (!launch_.has_value()) {
                throw std::invalid_argument("KernelDescriptor requires launch callable");
            }

            using InputTuple = std::decay_t<std::invoke_result_t<InputFactory&>>;
            using ExpectedVector = std::decay_t<std::invoke_result_t<ExpectedFactory&>>;
            using OutputValue = typename ExpectedVector::value_type;
            using DeviceInputRefs = detail::device_buffer_refs_tuple_t<InputTuple>;

            static_assert(detail::is_tuple_of_vectors_v<InputTuple>,
                          "KernelDescriptor inputs factory must return std::tuple<std::vector<T>...>");
            static_assert(detail::is_std_vector_v<ExpectedVector>,
                          "KernelDescriptor expected factory must return std::vector<T>");
            static_assert(
                std::is_invocable_v<LaunchFn&,
                                    const core::KernelLaunchConfig&,
                                    DeviceInputRefs&,
                                    core::DeviceMemory<OutputValue>&>,
                "KernelDescriptor launch callable must accept "
                "(const core::KernelLaunchConfig&, DeviceInputs&, core::DeviceMemory<OutT>&)");

            if constexpr (!std::is_arithmetic_v<OutputValue>) {
                if (tolerance_.has_value()) {
                    throw std::invalid_argument(
                        "KernelDescriptor tolerance requires arithmetic output values");
                }
            }

            using Model = detail::KernelDescriptorModel<InputFactory, ExpectedFactory, LaunchFn>;
            return KernelDescriptor(std::make_shared<Model>(
                name_, *problem_size_, *inputs_, *expected_, tolerance_, *launch_));
        }
    }

private:
    KernelDescriptorBuilder(std::string name,
                            std::optional<std::size_t> problem_size,
                            std::optional<InputFactory> inputs,
                            std::optional<ExpectedFactory> expected,
                            std::optional<double> tolerance,
                            std::optional<LaunchFn> launch)
        : name_(std::move(name)),
          problem_size_(std::move(problem_size)),
          inputs_(std::move(inputs)),
          expected_(std::move(expected)),
          tolerance_(std::move(tolerance)),
          launch_(std::move(launch)) {
    }

    std::string name_;
    std::optional<std::size_t> problem_size_{};
    std::optional<InputFactory> inputs_{};
    std::optional<ExpectedFactory> expected_{};
    std::optional<double> tolerance_{};
    std::optional<LaunchFn> launch_{};

    template <typename OtherInputFactory, typename OtherExpectedFactory, typename OtherLaunchFn>
    friend class KernelDescriptorBuilder;
};

[[nodiscard]] inline auto describe_kernel(std::string name) {
    return KernelDescriptorBuilder<>(std::move(name));
}

} // namespace cuda_test::pipeline

namespace cuda_test {

using pipeline::KernelDescriptor;

[[nodiscard]] inline auto describe_kernel(std::string name) {
    return pipeline::describe_kernel(std::move(name));
}

} // namespace cuda_test

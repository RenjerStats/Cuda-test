#pragma once

#include "cuda_test/core/detail/cuda_compat.hpp"
#include "cuda_test/core/error.hpp"
#include "cuda_test/core/types.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace cuda_test::profiling {

enum class Stage : std::size_t {
    h2d = 0,
    kernel = 1,
    d2h = 2,
    total = 3
};

class StagedTimer {
public:
    StagedTimer() {
        initialize();
    }

    ~StagedTimer() {
        destroy();
    }

    StagedTimer(const StagedTimer&) = delete;
    StagedTimer& operator=(const StagedTimer&) = delete;
    // Event ownership stays local to the timer for now. If later phases need transfer semantics,
    // move support can be added with explicit handle nulling similar to DeviceMemory<T>.
    StagedTimer(StagedTimer&&) = delete;
    StagedTimer& operator=(StagedTimer&&) = delete;

    void start(Stage stage, cudaStream_t stream = nullptr) {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        State& state = states_[to_index(stage)];
        if (state.running) {
            throw std::logic_error("Cannot start a profiling stage that is already running");
        }

        CUDA_CHECK(cudaEventRecord(state.start_event, stream));
        state.running = true;
        state.completed = false;
#else
        (void)stage;
        (void)stream;
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    void stop(Stage stage, cudaStream_t stream = nullptr) {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        State& state = states_[to_index(stage)];
        if (!state.running) {
            throw std::logic_error("Cannot stop a profiling stage that was not started");
        }

        CUDA_CHECK(cudaEventRecord(state.stop_event, stream));
        // Phase 2 intentionally exposes a synchronous stop() API so callers immediately receive
        // a ready-to-use elapsed value without extra polling or stream coordination.
        CUDA_CHECK(cudaEventSynchronize(state.stop_event));

        float elapsed_ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&elapsed_ms, state.start_event, state.stop_event));

        assign(stage, static_cast<double>(elapsed_ms));
        state.running = false;
        state.completed = true;
#else
        (void)stage;
        (void)stream;
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    void start_h2d(cudaStream_t stream = nullptr) {
        start(Stage::h2d, stream);
    }

    void stop_h2d(cudaStream_t stream = nullptr) {
        stop(Stage::h2d, stream);
    }

    void start_kernel(cudaStream_t stream = nullptr) {
        start(Stage::kernel, stream);
    }

    void stop_kernel(cudaStream_t stream = nullptr) {
        stop(Stage::kernel, stream);
    }

    void start_d2h(cudaStream_t stream = nullptr) {
        start(Stage::d2h, stream);
    }

    void stop_d2h(cudaStream_t stream = nullptr) {
        stop(Stage::d2h, stream);
    }

    void start_total(cudaStream_t stream = nullptr) {
        start(Stage::total, stream);
    }

    void stop_total(cudaStream_t stream = nullptr) {
        stop(Stage::total, stream);
    }

    void reset() noexcept {
        breakdown_ = {};
        clear_state_flags();
    }

    [[nodiscard]] core::ProfilingBreakdown breakdown() const noexcept {
        return breakdown_;
    }

private:
    struct State {
        cudaEvent_t start_event = nullptr;
        cudaEvent_t stop_event = nullptr;
        bool running = false;
        bool completed = false;
    };

    static constexpr std::size_t stage_count = 4;

    static constexpr std::size_t to_index(Stage stage) noexcept {
        return static_cast<std::size_t>(stage);
    }

    void initialize() {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        for (State& state : states_) {
            CUDA_CHECK(cudaEventCreate(&state.start_event));
            CUDA_CHECK(cudaEventCreate(&state.stop_event));
        }
#endif
    }

    void destroy() noexcept {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        for (State& state : states_) {
            if (state.start_event != nullptr) {
                (void)cudaEventDestroy(state.start_event);
            }
            if (state.stop_event != nullptr) {
                (void)cudaEventDestroy(state.stop_event);
            }
        }
#endif
        clear_events();
        clear_state_flags();
        breakdown_ = {};
    }

    void clear_events() noexcept {
        for (State& state : states_) {
            state.start_event = nullptr;
            state.stop_event = nullptr;
        }
    }

    void clear_state_flags() noexcept {
        for (State& state : states_) {
            state.running = false;
            state.completed = false;
        }
    }

    void assign(Stage stage, double elapsed_ms) noexcept {
        switch (stage) {
        case Stage::h2d:
            breakdown_.h2d_ms = elapsed_ms;
            break;
        case Stage::kernel:
            breakdown_.kernel_ms = elapsed_ms;
            break;
        case Stage::d2h:
            breakdown_.d2h_ms = elapsed_ms;
            break;
        case Stage::total:
            breakdown_.total_ms = elapsed_ms;
            break;
        }
    }

    std::array<State, stage_count> states_{};
    core::ProfilingBreakdown breakdown_{};
};

} // namespace cuda_test::profiling

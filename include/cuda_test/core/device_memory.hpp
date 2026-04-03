#pragma once

#include "cuda_test/core/error.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cuda_test::core {

template <typename T>
class DeviceMemory {
public:
    DeviceMemory() = default;

    explicit DeviceMemory(std::size_t size) {
        reset(size);
    }

    ~DeviceMemory() {
        release();
    }

    DeviceMemory(const DeviceMemory&) = delete;
    DeviceMemory& operator=(const DeviceMemory&) = delete;

    DeviceMemory(DeviceMemory&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    DeviceMemory& operator=(DeviceMemory&& other) noexcept {
        if (this != &other) {
            release();
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }

        return *this;
    }

    void reset(std::size_t size = 0) {
        release();

        if (size == 0) {
            return;
        }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        T* new_data = nullptr;
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&new_data), size * sizeof(T)));
        data_ = new_data;
        size_ = size;
#else
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    [[nodiscard]] T* data() noexcept {
        return data_;
    }

    [[nodiscard]] const T* data() const noexcept {
        return data_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] std::size_t bytes() const noexcept {
        return size_ * sizeof(T);
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    void copy_from_host(const T* host_data, std::size_t count) {
        validate_copy_args(host_data, count);

        if (count == 0) {
            return;
        }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        CUDA_CHECK(cudaMemcpy(data_, host_data, count * sizeof(T), cudaMemcpyHostToDevice));
#else
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    void copy_from_host(const std::vector<T>& host_data) {
        copy_from_host(host_data.data(), host_data.size());
    }

    void copy_to_host(T* host_data, std::size_t count) const {
        validate_copy_args(host_data, count);

        if (count == 0) {
            return;
        }

#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        CUDA_CHECK(cudaMemcpy(host_data, data_, count * sizeof(T), cudaMemcpyDeviceToHost));
#else
        throw std::runtime_error("CUDA support is disabled");
#endif
    }

    [[nodiscard]] std::vector<T> copy_to_host() const {
        std::vector<T> host_data(size_);
        copy_to_host(host_data.data(), host_data.size());
        return host_data;
    }

private:
    void release() noexcept {
#if defined(CUDA_TEST_HAS_CUDA) && CUDA_TEST_HAS_CUDA
        if (data_ != nullptr) {
            (void)cudaFree(data_);
        }
#endif
        data_ = nullptr;
        size_ = 0;
    }

    void validate_copy_args(const T* host_data, std::size_t count) const {
        if (count != size_) {
            throw std::invalid_argument("Host/device copy size mismatch");
        }

        if (count > 0 && host_data == nullptr) {
            throw std::invalid_argument("Host pointer must not be null for non-empty copies");
        }
    }

    T* data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace cuda_test::core

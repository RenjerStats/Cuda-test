#pragma once

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace cuda_test::testing {

namespace detail {

template <typename T>
inline void validate_array_inputs(const T* actual, const T* expected, std::size_t size) {
    if (size > 0 && (actual == nullptr || expected == nullptr)) {
        throw std::invalid_argument("Validation inputs must not be null for non-empty ranges");
    }
}

} // namespace detail

template <typename T>
inline void expect_array_eq(const T* actual, const T* expected, std::size_t size) {
    detail::validate_array_inputs(actual, expected, size);

    for (std::size_t index = 0; index < size; ++index) {
        if (!(actual[index] == expected[index])) {
            ADD_FAILURE() << "Array mismatch at index " << index << ": actual=" << actual[index]
                          << ", expected=" << expected[index];
        }
    }
}

template <typename T>
inline void expect_array_near(const T* actual, const T* expected, std::size_t size, T eps) {
    detail::validate_array_inputs(actual, expected, size);

    for (std::size_t index = 0; index < size; ++index) {
        const T delta = static_cast<T>(std::fabs(actual[index] - expected[index]));
        if (delta > eps) {
            ADD_FAILURE() << "Array mismatch at index " << index << ": actual=" << actual[index]
                          << ", expected=" << expected[index] << ", delta=" << delta
                          << ", eps=" << eps;
        }
    }
}

} // namespace cuda_test::testing

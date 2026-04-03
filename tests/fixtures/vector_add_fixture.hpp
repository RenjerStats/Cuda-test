#pragma once

#include <cstddef>
#include <numeric>
#include <vector>

namespace cuda_test::tests::fixtures {

struct VectorAddFixtureData {
    std::vector<float> lhs;
    std::vector<float> rhs;
    std::vector<float> expected;
};

inline VectorAddFixtureData make_vector_add_fixture(std::size_t size) {
    VectorAddFixtureData data;
    data.lhs.resize(size);
    data.rhs.resize(size);
    data.expected.resize(size);

    std::iota(data.lhs.begin(), data.lhs.end(), 1.0f);
    std::iota(data.rhs.begin(), data.rhs.end(), 10.0f);

    for (std::size_t index = 0; index < size; ++index) {
        data.expected[index] = data.lhs[index] + data.rhs[index];
    }

    return data;
}

} // namespace cuda_test::tests::fixtures

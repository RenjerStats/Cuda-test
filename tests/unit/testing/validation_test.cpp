#include "cuda_test/testing/validation.hpp"

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace cuda_test::testing {
namespace {

TEST(ValidationTest, ExpectArrayEqPassesForIdenticalArrays) {
    const int actual[] = {1, 2, 3, 4};
    const int expected[] = {1, 2, 3, 4};

    expect_array_eq(actual, expected, 4);
}

TEST(ValidationTest, ExpectArrayEqReportsFailingIndex) {
    const int actual[] = {1, 2, 99, 4};
    const int expected[] = {1, 2, 3, 4};

    EXPECT_NONFATAL_FAILURE(expect_array_eq(actual, expected, 4), "index 2");
}

TEST(ValidationTest, ExpectArrayNearPassesWithinTolerance) {
    const float actual[] = {1.0f, 2.0f, 3.000001f};
    const float expected[] = {1.0f, 2.0f, 3.0f};

    expect_array_near(actual, expected, 3, 1e-5f);
}

TEST(ValidationTest, ExpectArrayNearReportsFailingIndex) {
    const float actual[] = {1.0f, 2.0f, 3.1f};
    const float expected[] = {1.0f, 2.0f, 3.0f};

    EXPECT_NONFATAL_FAILURE(expect_array_near(actual, expected, 3, 1e-5f), "index 2");
}

TEST(ValidationTest, RejectsNullPointersForNonEmptyRanges) {
    const int expected[] = {1, 2, 3};

    EXPECT_THROW(expect_array_eq<int>(nullptr, expected, 3), std::invalid_argument);
    EXPECT_THROW(expect_array_near<float>(nullptr, nullptr, 1, 1e-5f), std::invalid_argument);
}

} // namespace
} // namespace cuda_test::testing

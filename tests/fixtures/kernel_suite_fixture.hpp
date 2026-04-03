#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace cuda_test::fixtures {

inline std::vector<float> make_float_sequence(std::size_t size, float start, float step) {
    std::vector<float> values(size);
    float current = start;
    for (float& value : values) {
        value = current;
        current += step;
    }
    return values;
}

inline std::vector<int> make_int_sequence(std::size_t size, int start, int step) {
    std::vector<int> values(size);
    int current = start;
    for (int& value : values) {
        value = current;
        current += step;
    }
    return values;
}

struct DensityUpdateFixture {
    std::vector<float> density;
    std::vector<float> delta;
    std::vector<float> expected;

    [[nodiscard]] std::size_t size() const noexcept {
        return density.size();
    }
};

inline DensityUpdateFixture make_density_update_fixture(std::size_t size) {
    DensityUpdateFixture fixture;
    fixture.density = make_float_sequence(size, 0.25f, 0.5f);
    fixture.delta = make_float_sequence(size, -0.75f, 0.125f);
    fixture.expected.resize(size);

    for (std::size_t index = 0; index < size; ++index) {
        fixture.expected[index] = fixture.density[index] + fixture.delta[index];
    }

    return fixture;
}

struct PhysicsIntegrationFixture {
    std::vector<float> position;
    std::vector<float> velocity;
    float dt = 0.125f;
    std::vector<float> expected;

    [[nodiscard]] std::size_t size() const noexcept {
        return position.size();
    }
};

inline PhysicsIntegrationFixture make_physics_integration_fixture(std::size_t size) {
    PhysicsIntegrationFixture fixture;
    fixture.position = make_float_sequence(size, -2.0f, 0.25f);
    fixture.velocity = make_float_sequence(size, 1.0f, -0.05f);
    fixture.expected.resize(size);

    for (std::size_t index = 0; index < size; ++index) {
        fixture.expected[index] = fixture.position[index] + fixture.velocity[index] * fixture.dt;
    }

    return fixture;
}

struct ContactFlagFixture {
    std::vector<float> lhs;
    std::vector<float> rhs;
    float threshold = 0.35f;
    std::vector<int> expected;

    [[nodiscard]] std::size_t size() const noexcept {
        return lhs.size();
    }
};

inline ContactFlagFixture make_contact_flag_fixture(std::size_t size) {
    ContactFlagFixture fixture;
    fixture.lhs = make_float_sequence(size, -1.0f, 0.1f);
    fixture.rhs = make_float_sequence(size, -0.85f, 0.12f);
    fixture.expected.resize(size);

    for (std::size_t index = 0; index < size; ++index) {
        fixture.expected[index] =
            std::fabs(fixture.lhs[index] - fixture.rhs[index]) <= fixture.threshold ? 1 : 0;
    }

    return fixture;
}

struct ActiveCompactionFixture {
    std::vector<int> active_mask;
    std::vector<int> expected_indices;
    int expected_count = 0;

    [[nodiscard]] std::size_t size() const noexcept {
        return active_mask.size();
    }
};

inline ActiveCompactionFixture make_active_compaction_fixture(std::size_t size) {
    ActiveCompactionFixture fixture;
    fixture.active_mask.resize(size);

    for (std::size_t index = 0; index < size; ++index) {
        const int active = ((index % 3U) == 0U || (index % 5U) == 0U) ? 1 : 0;
        fixture.active_mask[index] = active;
        if (active != 0) {
            fixture.expected_indices.push_back(static_cast<int>(index));
        }
    }

    fixture.expected_count = static_cast<int>(fixture.expected_indices.size());
    return fixture;
}

struct BufferGenerationFixture {
    std::vector<int> source;
    int multiplier = 3;
    int offset = 7;
    std::vector<int> expected;

    [[nodiscard]] std::size_t size() const noexcept {
        return source.size();
    }
};

inline BufferGenerationFixture make_buffer_generation_fixture(std::size_t size) {
    BufferGenerationFixture fixture;
    fixture.source = make_int_sequence(size, 2, 3);
    fixture.expected.resize(size);

    for (std::size_t index = 0; index < size; ++index) {
        fixture.expected[index] = fixture.source[index] * fixture.multiplier + fixture.offset;
    }

    return fixture;
}

struct IntersectionFixture {
    std::vector<float> min_values;
    std::vector<float> max_values;
    float query = 1.25f;
    std::vector<int> expected;

    [[nodiscard]] std::size_t size() const noexcept {
        return min_values.size();
    }
};

inline IntersectionFixture make_intersection_fixture(std::size_t size) {
    IntersectionFixture fixture;
    fixture.min_values = make_float_sequence(size, -0.5f, 0.1f);
    fixture.max_values.resize(size);
    fixture.expected.resize(size);

    for (std::size_t index = 0; index < size; ++index) {
        fixture.max_values[index] = fixture.min_values[index] + 1.5f + static_cast<float>(index % 3U) * 0.1f;
        fixture.expected[index] =
            (fixture.query >= fixture.min_values[index] && fixture.query <= fixture.max_values[index]) ? 1 : 0;
    }

    return fixture;
}

inline DensityUpdateFixture make_density_update_smallest_fixture() {
    return make_density_update_fixture(1);
}

inline PhysicsIntegrationFixture make_physics_integration_smallest_fixture() {
    return make_physics_integration_fixture(1);
}

inline ContactFlagFixture make_contact_flag_smallest_fixture() {
    return make_contact_flag_fixture(2);
}

inline ActiveCompactionFixture make_active_compaction_smallest_fixture() {
    return make_active_compaction_fixture(4);
}

inline BufferGenerationFixture make_buffer_generation_smallest_fixture() {
    return make_buffer_generation_fixture(1);
}

inline IntersectionFixture make_intersection_smallest_fixture() {
    return make_intersection_fixture(2);
}

} // namespace cuda_test::fixtures

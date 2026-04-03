#include "cuda_test/autotune/search.hpp"
#include "cuda_test/core/device_info.hpp"
#include "cuda_test/testing/kernel_test_fixture.hpp"
#include "examples/kernels/kernel_suite.hpp"
#include "tests/fixtures/kernel_suite_fixture.hpp"

#include <gtest/gtest.h>

#include <string>

namespace cuda_test {
namespace {

class KernelIntegrationTest : public testing::KernelTestFixture {
protected:
    void SetUp() override {
        if (core::device_count() <= 0) {
            GTEST_SKIP() << "No CUDA device is visible to the runtime";
        }

        testing::KernelTestFixture::SetUp();
    }
};

TEST_F(KernelIntegrationTest, RepresentativeFixturesMatchHostReferencesForAllKernels) {
    examples::kernels::DensityUpdateCase density_case(fixtures::make_density_update_fixture(2048));
    examples::kernels::PhysicsIntegrationCase physics_case(
        fixtures::make_physics_integration_fixture(2048));
    examples::kernels::ContactFlagCase contact_case(fixtures::make_contact_flag_fixture(2048));
    examples::kernels::ActiveCompactionCase compaction_case(
        fixtures::make_active_compaction_fixture(2048));
    examples::kernels::BufferGenerationCase buffer_case(fixtures::make_buffer_generation_fixture(2048));
    examples::kernels::IntervalIntersectionCase intersection_case(
        fixtures::make_intersection_fixture(2048));

    {
        SCOPED_TRACE(density_case.name());
        EXPECT_TRUE(density_case.validate(density_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(physics_case.name());
        EXPECT_TRUE(physics_case.validate(physics_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(contact_case.name());
        EXPECT_TRUE(contact_case.validate(contact_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(compaction_case.name());
        EXPECT_TRUE(compaction_case.validate(compaction_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(buffer_case.name());
        EXPECT_TRUE(buffer_case.validate(buffer_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(intersection_case.name());
        EXPECT_TRUE(intersection_case.validate(intersection_case.baseline_config(selected_device_id())));
    }
}

TEST_F(KernelIntegrationTest, SmallestMeaningfulFixturesMatchHostReferencesForAllKernels) {
    examples::kernels::DensityUpdateCase density_case(
        fixtures::make_density_update_smallest_fixture());
    examples::kernels::PhysicsIntegrationCase physics_case(
        fixtures::make_physics_integration_smallest_fixture());
    examples::kernels::ContactFlagCase contact_case(fixtures::make_contact_flag_smallest_fixture());
    examples::kernels::ActiveCompactionCase compaction_case(
        fixtures::make_active_compaction_smallest_fixture());
    examples::kernels::BufferGenerationCase buffer_case(
        fixtures::make_buffer_generation_smallest_fixture());
    examples::kernels::IntervalIntersectionCase intersection_case(
        fixtures::make_intersection_smallest_fixture());

    {
        SCOPED_TRACE(density_case.name());
        EXPECT_TRUE(density_case.validate(density_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(physics_case.name());
        EXPECT_TRUE(physics_case.validate(physics_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(contact_case.name());
        EXPECT_TRUE(contact_case.validate(contact_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(compaction_case.name());
        EXPECT_TRUE(compaction_case.validate(compaction_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(buffer_case.name());
        EXPECT_TRUE(buffer_case.validate(buffer_case.baseline_config(selected_device_id())));
    }
    {
        SCOPED_TRACE(intersection_case.name());
        EXPECT_TRUE(intersection_case.validate(intersection_case.baseline_config(selected_device_id())));
    }
}

TEST_F(KernelIntegrationTest, BaselineLaunchConfigCoversRepresentativeInputs) {
    const auto density_case = examples::kernels::DensityUpdateCase(fixtures::make_density_update_fixture(2048));
    const auto physics_case =
        examples::kernels::PhysicsIntegrationCase(fixtures::make_physics_integration_fixture(2048));
    const auto contact_case = examples::kernels::ContactFlagCase(fixtures::make_contact_flag_fixture(2048));
    const auto compaction_case =
        examples::kernels::ActiveCompactionCase(fixtures::make_active_compaction_fixture(2048));
    const auto buffer_case =
        examples::kernels::BufferGenerationCase(fixtures::make_buffer_generation_fixture(2048));
    const auto intersection_case =
        examples::kernels::IntervalIntersectionCase(fixtures::make_intersection_fixture(2048));

    {
        const auto config = density_case.baseline_config(selected_device_id());
        EXPECT_TRUE(examples::kernels::launch_covers_problem(config, density_case.problem_size()));
    }
    {
        const auto config = physics_case.baseline_config(selected_device_id());
        EXPECT_TRUE(examples::kernels::launch_covers_problem(config, physics_case.problem_size()));
    }
    {
        const auto config = contact_case.baseline_config(selected_device_id());
        EXPECT_TRUE(examples::kernels::launch_covers_problem(config, contact_case.problem_size()));
    }
    {
        const auto config = compaction_case.baseline_config(selected_device_id());
        EXPECT_TRUE(examples::kernels::launch_covers_problem(config, compaction_case.problem_size()));
    }
    {
        const auto config = buffer_case.baseline_config(selected_device_id());
        EXPECT_TRUE(examples::kernels::launch_covers_problem(config, buffer_case.problem_size()));
    }
    {
        const auto config = intersection_case.baseline_config(selected_device_id());
        EXPECT_TRUE(examples::kernels::launch_covers_problem(config, intersection_case.problem_size()));
    }
}

TEST_F(KernelIntegrationTest, AutotuneProducesWinnerForAllRepresentativeKernels) {
    auto spec = examples::kernels::make_default_autotune_spec(selected_device_id());
    spec.warmup_runs = 1;
    spec.measure_runs = 3;

    examples::kernels::DensityUpdateCase density_case(fixtures::make_density_update_fixture(1024));
    examples::kernels::PhysicsIntegrationCase physics_case(
        fixtures::make_physics_integration_fixture(1024));
    examples::kernels::ContactFlagCase contact_case(fixtures::make_contact_flag_fixture(1024));
    examples::kernels::ActiveCompactionCase compaction_case(
        fixtures::make_active_compaction_fixture(1024));
    examples::kernels::BufferGenerationCase buffer_case(fixtures::make_buffer_generation_fixture(1024));
    examples::kernels::IntervalIntersectionCase intersection_case(
        fixtures::make_intersection_fixture(1024));

    const auto density_result = autotune::tune_kernel(
        spec,
        density_case.problem_size(),
        [&density_case](const core::KernelLaunchConfig& config) { return density_case.measure(config); },
        [&density_case](const core::KernelLaunchConfig& config) { return density_case.validate(config); });
    EXPECT_FALSE(density_result.reason.empty());
    EXPECT_FALSE(density_result.all_candidates.empty());

    const auto physics_result = autotune::tune_kernel(
        spec,
        physics_case.problem_size(),
        [&physics_case](const core::KernelLaunchConfig& config) { return physics_case.measure(config); },
        [&physics_case](const core::KernelLaunchConfig& config) { return physics_case.validate(config); });
    EXPECT_FALSE(physics_result.reason.empty());
    EXPECT_FALSE(physics_result.all_candidates.empty());

    const auto contact_result = autotune::tune_kernel(
        spec,
        contact_case.problem_size(),
        [&contact_case](const core::KernelLaunchConfig& config) { return contact_case.measure(config); },
        [&contact_case](const core::KernelLaunchConfig& config) { return contact_case.validate(config); });
    EXPECT_FALSE(contact_result.reason.empty());
    EXPECT_FALSE(contact_result.all_candidates.empty());

    const auto compaction_result = autotune::tune_kernel(
        spec,
        compaction_case.problem_size(),
        [&compaction_case](const core::KernelLaunchConfig& config) {
            return compaction_case.measure(config);
        },
        [&compaction_case](const core::KernelLaunchConfig& config) { return compaction_case.validate(config); });
    EXPECT_FALSE(compaction_result.reason.empty());
    EXPECT_FALSE(compaction_result.all_candidates.empty());

    const auto buffer_result = autotune::tune_kernel(
        spec,
        buffer_case.problem_size(),
        [&buffer_case](const core::KernelLaunchConfig& config) { return buffer_case.measure(config); },
        [&buffer_case](const core::KernelLaunchConfig& config) { return buffer_case.validate(config); });
    EXPECT_FALSE(buffer_result.reason.empty());
    EXPECT_FALSE(buffer_result.all_candidates.empty());

    const auto intersection_result = autotune::tune_kernel(
        spec,
        intersection_case.problem_size(),
        [&intersection_case](const core::KernelLaunchConfig& config) {
            return intersection_case.measure(config);
        },
        [&intersection_case](const core::KernelLaunchConfig& config) {
            return intersection_case.validate(config);
        });
    EXPECT_FALSE(intersection_result.reason.empty());
    EXPECT_FALSE(intersection_result.all_candidates.empty());
}

} // namespace
} // namespace cuda_test

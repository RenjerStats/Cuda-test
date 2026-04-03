#include "cuda_test/analysis/metrics.hpp"
#include "cuda_test/autotune/search.hpp"
#include "cuda_test/core/device_info.hpp"
#include "cuda_test/reporting/export.hpp"
#include "examples/kernels/kernel_suite.hpp"
#include "tests/fixtures/kernel_suite_fixture.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using cuda_test::autotune::AutoTuneResult;
using cuda_test::core::KernelLaunchConfig;

struct SummaryRow {
    std::string kernel_name;
    std::size_t input_size = 0;
    unsigned int baseline_block = 0;
    unsigned int baseline_grid = 0;
    double baseline_kernel_median_ms = 0.0;
    unsigned int best_block = 0;
    unsigned int best_grid = 0;
    double best_kernel_median_ms = 0.0;
    double best_total_median_ms = 0.0;
    double transfer_compute_ratio = 0.0;
    double speedup_vs_baseline = 0.0;
    std::string reason;
};

std::string format_double(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    return stream.str();
}

bool same_config(const KernelLaunchConfig& lhs, const KernelLaunchConfig& rhs) {
    return lhs.grid.x == rhs.grid.x && lhs.block.x == rhs.block.x && lhs.shared_mem == rhs.shared_mem &&
           lhs.device_id == rhs.device_id;
}

const cuda_test::autotune::CandidateRecord* find_baseline_candidate(const AutoTuneResult& result,
                                                                    const KernelLaunchConfig& baseline) {
    for (const auto& candidate : result.all_candidates) {
        if (same_config(candidate.config, baseline)) {
            return &candidate;
        }
    }

    return nullptr;
}

template <typename KernelCase>
SummaryRow run_case(KernelCase& kernel_case,
                    int device_id,
                    const std::filesystem::path& output_directory) {
    auto spec = cuda_test::examples::kernels::make_default_autotune_spec(device_id);
    const KernelLaunchConfig baseline = kernel_case.baseline_config(device_id);

    const AutoTuneResult result = cuda_test::autotune::tune_kernel(
        spec,
        kernel_case.problem_size(),
        [&kernel_case](const KernelLaunchConfig& config) { return kernel_case.measure(config); },
        [&kernel_case](const KernelLaunchConfig& config) { return kernel_case.validate(config); });

    const auto* baseline_candidate = find_baseline_candidate(result, baseline);
    if (baseline_candidate == nullptr) {
        throw std::runtime_error("Baseline candidate missing from autotune result");
    }

    const std::filesystem::path csv_path = output_directory / (kernel_case.name() + "-autotune.csv");
    const std::filesystem::path json_path = output_directory / (kernel_case.name() + "-autotune.json");
    cuda_test::reporting::export_csv(csv_path, result);
    cuda_test::reporting::export_json(json_path, result);

    SummaryRow row;
    row.kernel_name = kernel_case.name();
    row.input_size = kernel_case.problem_size();
    row.baseline_block = baseline.block.x;
    row.baseline_grid = baseline.grid.x;
    row.baseline_kernel_median_ms = baseline_candidate->benchmark.kernel_stats.median_ms;
    row.best_block = result.best.block.x;
    row.best_grid = result.best.grid.x;
    row.best_kernel_median_ms = result.stats.median_ms;
    row.best_total_median_ms = 0.0;
    for (const auto& candidate : result.all_candidates) {
        if (same_config(candidate.config, result.best)) {
            row.best_total_median_ms = candidate.benchmark.total_stats.median_ms;
            const cuda_test::core::ProfilingBreakdown best_breakdown{
                candidate.benchmark.h2d_stats.median_ms,
                candidate.benchmark.kernel_stats.median_ms,
                candidate.benchmark.d2h_stats.median_ms,
                candidate.benchmark.total_stats.median_ms,
            };
            row.transfer_compute_ratio = cuda_test::analysis::transfer_compute_ratio(best_breakdown);
            break;
        }
    }
    row.speedup_vs_baseline = baseline_candidate->benchmark.kernel_stats.median_ms / result.stats.median_ms;
    row.reason = result.reason;
    return row;
}

void write_summary_report(const std::filesystem::path& path,
                          const cuda_test::core::DeviceInfo& device_info,
                          const std::vector<SummaryRow>& rows) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open kernel integration summary report");
    }

    stream.imbue(std::locale::classic());
    stream << "kernel,input_size,device_name,baseline_block,baseline_grid,baseline_kernel_median_ms,"
              "best_block,best_grid,best_kernel_median_ms,best_total_median_ms,transfer_compute_ratio,"
              "speedup_vs_baseline,hardware_scope,reason\n";

    for (const SummaryRow& row : rows) {
        stream << row.kernel_name << ',' << row.input_size << ",\"" << device_info.name << "\"," << row.baseline_block
               << ',' << row.baseline_grid << ',' << format_double(row.baseline_kernel_median_ms) << ','
               << row.best_block << ',' << row.best_grid << ',' << format_double(row.best_kernel_median_ms) << ','
               << format_double(row.best_total_median_ms) << ',' << format_double(row.transfer_compute_ratio) << ','
               << format_double(row.speedup_vs_baseline) << ",single_gpu_only,\"" << row.reason << "\"\n";
    }
}

void write_hardware_note(const std::filesystem::path& path, const cuda_test::core::DeviceInfo& device_info) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open hardware note report");
    }

    stream.imbue(std::locale::classic());
    stream << "Phase 7 evidence was collected on one available GPU: " << device_info.name << " (device_id="
           << device_info.device_id << ").\n";
    stream << "A second-GPU run is deferred because the current laptop environment exposes only one NVIDIA GPU.\n";
}

} // namespace

int main() {
    using namespace cuda_test;

    if (core::device_count() <= 0) {
        std::cerr << "kernel_integration_runner: no CUDA device is visible to the runtime\n";
        return 1;
    }

    const int device_id = 0;
    const core::DeviceInfo device_info = core::get_device_info(device_id);
    const std::filesystem::path output_directory = std::filesystem::path("reports") / "tables" / "single-gpu";

    try {
        examples::kernels::DensityUpdateCase density_case(fixtures::make_density_update_fixture(4096));
        examples::kernels::PhysicsIntegrationCase physics_case(
            fixtures::make_physics_integration_fixture(4096));
        examples::kernels::ContactFlagCase contact_case(fixtures::make_contact_flag_fixture(4096));
        examples::kernels::ActiveCompactionCase compaction_case(
            fixtures::make_active_compaction_fixture(4096));
        examples::kernels::BufferGenerationCase buffer_case(
            fixtures::make_buffer_generation_fixture(4096));
        examples::kernels::IntervalIntersectionCase intersection_case(
            fixtures::make_intersection_fixture(4096));

        std::vector<SummaryRow> rows;
        rows.push_back(run_case(density_case, device_id, output_directory));
        rows.push_back(run_case(physics_case, device_id, output_directory));
        rows.push_back(run_case(contact_case, device_id, output_directory));
        rows.push_back(run_case(compaction_case, device_id, output_directory));
        rows.push_back(run_case(buffer_case, device_id, output_directory));
        rows.push_back(run_case(intersection_case, device_id, output_directory));

        write_summary_report(output_directory / "kernel-suite-summary.csv", device_info, rows);
        write_hardware_note(output_directory / "hardware-note.txt", device_info);

        std::cout << "Generated Phase 7 single-GPU reports in " << output_directory.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "kernel_integration_runner failed: " << error.what() << '\n';
        return 2;
    }
}

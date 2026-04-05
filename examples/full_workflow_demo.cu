#include "cuda_test/cuda_test.hpp"

#include <cuda_runtime.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kLargeProblemSize = 1U << 20U;
constexpr std::size_t kSmallProblemSize = 1U << 18U;
constexpr float kAlphaDefault = 2.5F;
constexpr float kAlphaAlt = -1.25F;

__global__ void saxpy_kernel(const float* x,
                             const float* y,
                             float* out,
                             int size,
                             float alpha) {
    const int index = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (index < size) {
        out[index] = alpha * x[index] + y[index];
    }
}

std::vector<float> make_x_values(std::size_t size) {
    std::vector<float> values(size);
    for (std::size_t index = 0; index < size; ++index) {
        values[index] = static_cast<float>((index % 251U) * 0.125F);
    }
    return values;
}

std::vector<float> make_y_values(std::size_t size) {
    std::vector<float> values(size);
    for (std::size_t index = 0; index < size; ++index) {
        values[index] = static_cast<float>((index % 97U) * 0.25F - 7.0F);
    }
    return values;
}

std::vector<float> make_expected_values(std::size_t size, float alpha) {
    const std::vector<float> x = make_x_values(size);
    const std::vector<float> y = make_y_values(size);

    std::vector<float> expected(size);
    for (std::size_t index = 0; index < size; ++index) {
        expected[index] = alpha * x[index] + y[index];
    }
    return expected;
}

cuda_test::KernelDescriptor make_saxpy_descriptor(std::string name,
                                                  std::size_t size,
                                                  float alpha) {
    return cuda_test::describe_kernel(std::move(name))
        .problem_size(size)
        .inputs([size]() {
            // The descriptor owns the host-side fixtures.
            // Each validate()/measure() call gets a fresh copy, which keeps the run deterministic
            // and makes correctness/benchmark/autotune independent from each other.
            return std::make_tuple(make_x_values(size), make_y_values(size));
        })
        .expected([size, alpha]() {
            // This is the CPU ground truth that correctness() compares against.
            return make_expected_values(size, alpha);
        })
        .tolerance(1e-5)
        .launch([size, alpha](const cuda_test::core::KernelLaunchConfig& config,
                              auto& device_inputs,
                              cuda_test::core::DeviceMemory<float>& output_device) {
            auto& x_device = std::get<0>(device_inputs);
            auto& y_device = std::get<1>(device_inputs);

            // The library decides grid/block/shared_mem/device_id for every run.
            // The launch callback is intentionally small: take those parameters and invoke the kernel.
            saxpy_kernel<<<config.grid, config.block, config.shared_mem>>>(
                x_device.data(),
                y_device.data(),
                output_device.data(),
                static_cast<int>(size),
                alpha);
        })
        .build();
}

std::filesystem::path demo_output_root() {
    return std::filesystem::current_path() / "reports" / "demo" / "full-workflow";
}

void print_heading(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

std::string format_ms(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value << " ms";
    return stream.str();
}

void print_recommendations(const cuda_test::PipelineReport& report) {
    if (!report.diagnose_enabled()) {
        std::cout << "Diagnostics were not enabled.\n";
        return;
    }

    if (report.recommendations().empty()) {
        std::cout << "No recommendations generated.\n";
        return;
    }

    for (const auto& recommendation : report.recommendations()) {
        std::cout << "- [" << recommendation.severity << "] " << recommendation.tag << '\n'
                  << "  summary    : " << recommendation.summary << '\n'
                  << "  suggestion : " << recommendation.suggestion << '\n';
    }
}

void print_pipeline_summary(const cuda_test::PipelineReport& report) {
    print_heading("Pipeline Summary");
    std::cout << "kernel              : " << report.kernel_name() << '\n'
              << "device_id           : " << report.device_id() << '\n'
              << "correctness enabled : " << (report.correctness_enabled() ? "yes" : "no") << '\n'
              << "correctness passed  : " << (report.correctness_passed() ? "yes" : "no") << '\n'
              << "pipeline passed     : " << (report.passed() ? "yes" : "no") << '\n';

    if (report.benchmark_result().has_value()) {
        std::cout << "benchmark median    : "
                  << format_ms(report.benchmark_result()->kernel_stats.median_ms) << '\n'
                  << "benchmark p95       : "
                  << format_ms(report.benchmark_result()->kernel_stats.p95_ms) << '\n'
                  << "benchmark cv        : "
                  << std::fixed << std::setprecision(4)
                  << report.benchmark_result()->kernel_stats.cv << '\n';
    }

    if (report.autotune_result().has_value()) {
        const auto& best = report.autotune_result()->best;
        std::cout << "best autotune block : " << best.block.x << '\n'
                  << "best autotune grid  : " << best.grid.x << '\n'
                  << "autotune median     : "
                  << format_ms(report.autotune_result()->stats.median_ms) << '\n'
                  << "selection reason    : " << report.autotune_result()->reason << '\n';
    }

    if (report.fingerprint().has_value()) {
        const auto& fingerprint = *report.fingerprint();
        std::cout << "occupancy           : " << std::fixed << std::setprecision(3)
                  << fingerprint.occupancy << '\n'
                  << "bandwidth util      : " << fingerprint.bandwidth_utilization << '\n'
                  << "transfer/compute    : " << fingerprint.transfer_compute_ratio << '\n'
                  << "block sensitivity   : " << fingerprint.block_sensitivity << '\n'
                  << "recommendations     : " << report.recommendations().size() << '\n';
    }
}

void write_demo_readme(const std::filesystem::path& root,
                       const cuda_test::PipelineReport& report,
                       const cuda_test::SuiteReport& suite_report) {
    std::ofstream stream(root / "README.txt", std::ios::binary | std::ios::trunc);
    stream << "cuda_test full workflow demo\n"
              "============================\n\n"
              "This directory was generated by examples/full_workflow_demo.cu.\n\n"
              "Artifacts\n"
              "---------\n"
              "- pipeline/pipeline_report.html : self-contained human-readable report\n"
              "- pipeline/pipeline_report.json : machine-readable pipeline envelope\n"
              "- pipeline/pipeline_report.csv  : flat summary for spreadsheets\n"
              "- pipeline/benchmark.json/.csv  : raw timing statistics\n"
              "- pipeline/autotune.json/.csv   : all accepted candidates and the winner\n"
              "- suite/suite_report.html       : one page with multiple kernel sections\n"
              "- suite/json/*.json             : one JSON file per kernel report\n"
              "- suite/csv/*.csv               : one CSV file per kernel report\n\n"
              "Key results\n"
              "-----------\n"
           << "pipeline_passed=" << (report.passed() ? "true" : "false") << '\n'
           << "benchmark_median_ms="
           << (report.benchmark_result().has_value() ? report.benchmark_result()->kernel_stats.median_ms : 0.0)
           << '\n'
           << "autotune_candidates="
           << (report.autotune_result().has_value() ? report.autotune_result()->all_candidates.size() : 0U)
           << '\n'
           << "recommendations=" << report.recommendations().size() << '\n'
           << "suite_reports=" << suite_report.reports().size() << '\n';
}

} // namespace

int main() try {
    const std::filesystem::path output_root = demo_output_root();
    std::filesystem::create_directories(output_root);

    // Step 1. Describe one CUDA kernel in a reusable form.
    // This object is the center of the developer workflow: it knows how to prepare inputs,
    // what output is expected, and how to launch the kernel under arbitrary launch configs.
    const cuda_test::KernelDescriptor saxpy_demo =
        make_saxpy_descriptor("demo_saxpy_f32", kLargeProblemSize, kAlphaDefault);

    // Step 2. Build the main pipeline.
    // This is the "single-kernel deep dive" path: correctness -> benchmark -> autotune -> diagnose.
    cuda_test::Pipeline pipeline = cuda_test::make_pipeline(saxpy_demo);
    pipeline.device(0)
        .correctness()
        .benchmark(cuda_test::benchmark::BenchmarkConfig{5, 30})
        .autotune(std::vector<int>{64, 128, 256, 512}, std::vector<int>{1, 2})
        .diagnose();

    const cuda_test::PipelineReport pipeline_report = pipeline.run();
    if (!pipeline_report.passed()) {
        std::cerr << "Pipeline failed correctness; demo stops before export.\n";
        return 1;
    }

    print_heading("What This Demo Covers");
    std::cout
        << "1. describe_kernel(): model one CUDA kernel with reusable host fixtures\n"
        << "2. make_pipeline(): run correctness, benchmark, autotune, diagnostics\n"
        << "3. export_json/csv/html(): persist artifacts for humans and tooling\n"
        << "4. make_suite(): batch multiple descriptors into one higher-level report\n";

    print_pipeline_summary(pipeline_report);

    print_heading("Diagnostics Recommendations");
    print_recommendations(pipeline_report);

    // Step 3. Export every artifact a developer would normally inspect.
    // The pipeline report gives the stitched high-level view.
    // Raw benchmark/autotune exports let you drill into the underlying measurements.
    const std::filesystem::path pipeline_dir = output_root / "pipeline";
    std::filesystem::create_directories(pipeline_dir);

    pipeline_report.to_json(pipeline_dir / "pipeline_report.json");
    pipeline_report.to_csv(pipeline_dir / "pipeline_report.csv");
    pipeline_report.to_html(pipeline_dir / "pipeline_report.html");

    if (pipeline_report.benchmark_result().has_value()) {
        cuda_test::reporting::export_json(
            pipeline_dir / "benchmark.json", *pipeline_report.benchmark_result());
        cuda_test::reporting::export_csv(
            pipeline_dir / "benchmark.csv", *pipeline_report.benchmark_result());
    }

    if (pipeline_report.autotune_result().has_value()) {
        cuda_test::reporting::export_json(
            pipeline_dir / "autotune.json", *pipeline_report.autotune_result());
        cuda_test::reporting::export_csv(
            pipeline_dir / "autotune.csv", *pipeline_report.autotune_result());
    }

    // Step 4. Show the "scale out" workflow.
    // A suite is what a developer would use when one kernel-level workflow works
    // and they want the same treatment for a family of kernels or problem variants.
    const cuda_test::KernelDescriptor small_variant =
        make_saxpy_descriptor("demo_saxpy_small_f32", kSmallProblemSize, kAlphaDefault);
    const cuda_test::KernelDescriptor alt_variant =
        make_saxpy_descriptor("demo_saxpy_negative_alpha_f32", kSmallProblemSize, kAlphaAlt);

    cuda_test::Suite suite = cuda_test::make_suite("demo_saxpy_suite");
    suite.device(0)
        .correctness()
        .benchmark(cuda_test::benchmark::BenchmarkConfig{5, 30})
        .autotune(std::vector<int>{64, 128, 256}, std::vector<int>{1, 2})
        .add(small_variant)
        .add(alt_variant);

    const cuda_test::SuiteReport suite_report = suite.run_all();
    const std::filesystem::path suite_dir = output_root / "suite";
    std::filesystem::create_directories(suite_dir);
    suite_report.to_html(suite_dir / "suite_report.html");
    suite_report.to_json(suite_dir / "json");
    suite_report.to_csv(suite_dir / "csv");

    write_demo_readme(output_root, pipeline_report, suite_report);

    print_heading("Generated Artifacts");
    std::cout << "- " << (pipeline_dir / "pipeline_report.html").string() << '\n'
              << "- " << (pipeline_dir / "pipeline_report.json").string() << '\n'
              << "- " << (pipeline_dir / "pipeline_report.csv").string() << '\n'
              << "- " << (pipeline_dir / "benchmark.json").string() << '\n'
              << "- " << (pipeline_dir / "benchmark.csv").string() << '\n'
              << "- " << (pipeline_dir / "autotune.json").string() << '\n'
              << "- " << (pipeline_dir / "autotune.csv").string() << '\n'
              << "- " << (suite_dir / "suite_report.html").string() << '\n'
              << "- " << (suite_dir / "json").string() << '\n'
              << "- " << (suite_dir / "csv").string() << '\n'
              << "- " << (output_root / "README.txt").string() << '\n';

    print_heading("How To Read The Output");
    std::cout
        << "- Start with pipeline_report.html for the narrative, charts, and recommendations.\n"
        << "- Use pipeline_report.json when you want a stable machine-readable envelope.\n"
        << "- Use benchmark/autotune CSV files when you want quick spreadsheet inspection.\n"
        << "- Use suite_report.html after you scale from one kernel to several related descriptors.\n";

    return 0;
} catch (const std::exception& error) {
    std::cerr << "Demo failed: " << error.what() << '\n';
    return 1;
}

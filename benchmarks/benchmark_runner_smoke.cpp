#include "cuda_test/benchmark/benchmark_runner.hpp"

#include <benchmark/benchmark.h>

namespace {

void benchmark_runner_synthetic(benchmark::State& state) {
    const cuda_test::benchmark::BenchmarkRunner runner;

    for (auto _ : state) {
        int invocation_count = 0;
        const cuda_test::benchmark::BenchmarkResult result = runner.run([&invocation_count]() {
            ++invocation_count;
            const double value = static_cast<double>(invocation_count);
            return cuda_test::core::ProfilingBreakdown{
                value,
                value + 1.0,
                value + 2.0,
                value + 3.0,
            };
        });
        benchmark::DoNotOptimize(result.total_stats.mean_ms);
    }
}

BENCHMARK(benchmark_runner_synthetic);

} // namespace

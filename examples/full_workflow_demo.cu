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
            // Descriptor владеет набором входных данных на стороне хоста.
            // Каждый вызов validate()/measure() получает свежую копию, что делает прогоны детерминированными
            // и отделяет проверку, бенчмарк и автотюнинг друг от друга.
            return std::make_tuple(make_x_values(size), make_y_values(size));
        })
        .expected([size, alpha]() {
            // Это CPU-эталон, с которым correctness() сравнивает результат ядра.
            return make_expected_values(size, alpha);
        })
        .tolerance(1e-5)
        .launch([size, alpha](const cuda_test::core::KernelLaunchConfig& config,
                              auto& device_inputs,
                              cuda_test::core::DeviceMemory<float>& output_device) {
            auto& x_device = std::get<0>(device_inputs);
            auto& y_device = std::get<1>(device_inputs);

            // Библиотека сама выбирает сетку, блок, разделяемую память и устройство для каждого прогона.
            // Launch-callback специально минимален: он получает параметры и просто вызывает ядро.
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
    stream << std::fixed << std::setprecision(5) << value << " ms";
    return stream.str();
}

std::string format_ratio(double value, int precision = 3) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string format_optional_ratio(double value, bool available, int precision = 3) {
    return available ? format_ratio(value, precision) : "н/д";
}

void print_recommendations(const cuda_test::PipelineReport& report) {
    if (!report.diagnose_enabled()) {
        std::cout << "Диагностика не была включена.\n";
        return;
    }

    if (report.recommendations().empty()) {
        std::cout << "Рекомендации не сгенерированы.\n";
        return;
    }

    for (const auto& recommendation : report.recommendations()) {
        std::cout << "- [" << recommendation.severity << "] " << recommendation.tag << '\n'
                  << "  суть        : " << recommendation.summary << '\n'
                  << "  совет       : " << recommendation.suggestion << '\n';
    }
}

void print_pipeline_summary(const cuda_test::PipelineReport& report) {
    print_heading("Сводка по пайплайну");
    std::cout << "ядро                : " << report.kernel_name() << '\n'
              << "идентификатор GPU   : " << report.device_id() << '\n'
              << "проверка включена   : " << (report.correctness_enabled() ? "да" : "нет") << '\n'
              << "проверка пройдена   : " << (report.correctness_passed() ? "да" : "нет") << '\n'
              << "пайплайн пройден    : " << (report.passed() ? "да" : "нет") << '\n';

    if (report.benchmark_result().has_value()) {
        std::cout << "медиана баз. замера : "
                  << format_ms(report.benchmark_result()->kernel_stats.median_ms) << '\n'
                  << "P95 баз. замера     : "
                  << format_ms(report.benchmark_result()->kernel_stats.p95_ms) << '\n'
                  << "КВ баз. замера      : "
                  << std::fixed << std::setprecision(4)
                  << report.benchmark_result()->kernel_stats.cv << '\n';
    }

    if (report.autotune_result().has_value()) {
        const auto& best = report.autotune_result()->best;
        std::cout << "лучший блок         : " << best.block.x << '\n'
                  << "лучшая сетка        : " << best.grid.x << '\n'
                  << "медиана тюнинга     : "
                  << format_ms(report.autotune_result()->stats.median_ms) << '\n'
                  << "пояснение выбора    : " << report.autotune_result()->reason << '\n';
    }

    if (report.fingerprint().has_value()) {
        const auto& fingerprint = *report.fingerprint();
        std::cout << "заполняемость       : "
                  << format_optional_ratio(fingerprint.occupancy, fingerprint.has_occupancy) << '\n'
                  << "util. проп. спос.   : "
                  << format_optional_ratio(
                         fingerprint.bandwidth_utilization, fingerprint.has_bandwidth_utilization)
                  << '\n'
                  << "передачи/вычисл.    : " << format_ratio(fingerprint.transfer_compute_ratio) << '\n'
                  << "чувств. к блоку     : " << format_ratio(fingerprint.block_sensitivity) << '\n'
                  << "рекомендаций        : " << report.recommendations().size() << '\n';
    }
}

void write_demo_readme(const std::filesystem::path& root,
                       const cuda_test::PipelineReport& report,
                       const cuda_test::SuiteReport& suite_report) {
    std::ofstream stream(root / "README.txt", std::ios::binary | std::ios::trunc);
    stream << "Демонстрация полного рабочего процесса cuda_test\n"
              "===============================================\n\n"
              "Этот каталог был сгенерирован примером examples/full_workflow_demo.cu.\n\n"
              "Артефакты\n"
              "---------\n"
              "- pipeline/pipeline_report.html : самостоятельный человекочитаемый отчет\n"
              "- pipeline/pipeline_report.json : машиночитаемый отчет по одному пайплайну\n"
              "- pipeline/pipeline_report.csv  : плоская сводка для таблиц\n"
              "- pipeline/benchmark.json/.csv  : сырая статистика измерений\n"
              "- pipeline/autotune.json/.csv   : все принятые кандидаты автотюнинга и победитель\n"
              "- suite/suite_report.html       : одна страница с несколькими разделами по ядрам\n"
              "- suite/json/*.json             : один JSON-файл на каждый отчет по ядру\n"
              "- suite/csv/*.csv               : один CSV-файл на каждый отчет по ядру\n\n"
              "Ключевые результаты\n"
              "-------------------\n"
           << "пайплайн_пройден="
           << (report.passed() ? "да" : "нет") << '\n'
           << "медиана_базового_замера_мс="
           << (report.benchmark_result().has_value() ? report.benchmark_result()->kernel_stats.median_ms : 0.0)
           << '\n'
           << "кандидатов_автотюнинга="
           << (report.autotune_result().has_value() ? report.autotune_result()->all_candidates.size() : 0U)
           << '\n'
           << "рекомендаций=" << report.recommendations().size() << '\n'
           << "отчетов_в_наборе=" << suite_report.reports().size() << '\n';
}

} // namespace

int main() try {
    const std::filesystem::path output_root = demo_output_root();
    std::filesystem::create_directories(output_root);

    // Шаг 1. Описываем одно CUDA-ядро в переиспользуемой форме.
    // Это центральный объект всего рабочего процесса: он знает, как подготовить входы,
    // какой результат считается корректным и как запускать ядро под произвольной конфигурацией запуска.
    const cuda_test::KernelDescriptor saxpy_demo =
        make_saxpy_descriptor("demo_saxpy_f32", kLargeProblemSize, kAlphaDefault);

    // Шаг 2. Собираем основной пайплайн.
    // Это путь "глубокого анализа одного ядра": проверка -> бенчмарк -> автотюнинг -> диагностика.
    cuda_test::Pipeline pipeline = cuda_test::make_pipeline(saxpy_demo);
    pipeline.device(0)
        .correctness()
        .benchmark(cuda_test::benchmark::BenchmarkConfig{5, 30})
        .autotune(std::vector<int>{64, 128, 256, 512}, std::vector<int>{1, 2})
        .diagnose();

    const cuda_test::PipelineReport pipeline_report = pipeline.run();
    if (!pipeline_report.passed()) {
        std::cerr << "Pipeline не прошел проверку корректности; демонстрация останавливается до экспорта.\n";
        return 1;
    }

    print_heading("Что показывает это демо");
    std::cout
        << "1. describe_kernel(): описывает CUDA-ядро с переиспользуемым набором входов\n"
        << "2. make_pipeline(): выполняет проверку, бенчмарк, автотюнинг и диагностику\n"
        << "3. export_json/csv/html(): сохраняет артефакты для людей и инструментов\n"
        << "4. make_suite(): объединяет несколько дескрипторов в один сводный отчет\n";

    print_pipeline_summary(pipeline_report);

    print_heading("Рекомендации диагностики");
    print_recommendations(pipeline_report);

    // Шаг 3. Экспортируем все артефакты, которые разработчик обычно смотрит руками.
    // Отчет по пайплайну дает цельную верхнеуровневую картину.
    // Сырые выгрузки бенчмарка и автотюнинга позволяют провалиться в исходные измерения.
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

    // Шаг 4. Показываем путь "масштабирования".
    // Suite нужен тогда, когда рабочий процесс для одного ядра уже работает,
    // и тот же подход нужно применить к семейству ядер или вариантам одной задачи.
    // Теперь он повторяет и этап диагностики, поэтому сводный HTML-отчет показывает
    // не только времена, но и рекомендации по каждому варианту.
    const cuda_test::KernelDescriptor small_variant =
        make_saxpy_descriptor("demo_saxpy_small_f32", kSmallProblemSize, kAlphaDefault);
    const cuda_test::KernelDescriptor alt_variant =
        make_saxpy_descriptor("demo_saxpy_negative_alpha_f32", kSmallProblemSize, kAlphaAlt);

    cuda_test::Suite suite = cuda_test::make_suite("demo_saxpy_suite");
    suite.device(0)
        .correctness()
        .benchmark(cuda_test::benchmark::BenchmarkConfig{5, 30})
        .autotune(std::vector<int>{64, 128, 256}, std::vector<int>{1, 2})
        .diagnose()
        .add(small_variant)
        .add(alt_variant);

    const cuda_test::SuiteReport suite_report = suite.run_all();
    const std::filesystem::path suite_dir = output_root / "suite";
    std::filesystem::create_directories(suite_dir);
    suite_report.to_html(suite_dir / "suite_report.html");
    suite_report.to_json(suite_dir / "json");
    suite_report.to_csv(suite_dir / "csv");

    write_demo_readme(output_root, pipeline_report, suite_report);

    print_heading("Сгенерированные артефакты");
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

    print_heading("Как читать результаты");
    std::cout
        << "- Начинайте с pipeline_report.html: там есть поясняющий текст, графики и рекомендации.\n"
        << "- Используйте pipeline_report.json, когда нужен стабильный машиночитаемый формат.\n"
        << "- Используйте CSV бенчмарка и автотюнинга, когда нужен быстрый просмотр в таблице.\n"
        << "- Используйте suite_report.html, когда переходите от одного ядра к нескольким связанным дескрипторам.\n";

    return 0;
} catch (const std::exception& error) {
    std::cerr << "Ошибка демо: " << error.what() << '\n';
    return 1;
}

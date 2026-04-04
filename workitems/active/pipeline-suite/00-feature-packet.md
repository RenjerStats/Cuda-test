# Feature Packet

## Task

- Name: Post-MVP Phase 2 — Pipeline & Suite
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/pipeline-suite`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` — решает проблему P2

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §11 (архитектура модулей), §13 (алгоритм автоподбора), §15 (профилирование и метрики), §18 (приёмка)
- Related section(s) in vision: «P2 — Отсутствие оркестрации», «Целевой UX — После», «Архитектурное решение: три слоя API»
- Dependency: принятая задача `workitems/active/kernel-descriptor/`

## Objective

Создать Layer 3-оркестратор поверх `KernelDescriptor`:

- `Pipeline` выполняет последовательность этапов `correctness -> benchmark -> autotune` одной командой `.run()`
- `Suite` выполняет один и тот же pipeline-профиль для нескольких descriptor'ов через `.run_all()`
- `PipelineReport` и `SuiteReport` предоставляют программный результат и сериализацию в JSON/CSV без ручного связывания `BenchmarkRunner`, `tune_kernel` и файлового вывода

Цель задачи — убрать ручную оркестрацию шагов из примеров и подготовить основу для следующих задач `export-refactor`, `analysis-expansion`, `diagnostics-advisor`.

## Scope

- In scope:
  - `include/cuda_test/pipeline/pipeline_report.hpp`
    - `PipelineReport`
    - `SuiteReport`
    - JSON/CSV export для этих report-типов
- `include/cuda_test/pipeline/pipeline.hpp`
    - `Pipeline`
    - свободная функция `make_pipeline(KernelDescriptor)`
  - `include/cuda_test/pipeline/suite.hpp`
    - `Suite`
    - свободная функция `make_suite(std::string)`
  - Fluent API `Pipeline`:
    - `.device(int device_id)` — устройство для baseline validate/benchmark и autotune spec
    - `.correctness()` — включить `descriptor.validate(...)`
    - `.benchmark(benchmark::BenchmarkConfig config = {})`
    - `.autotune(autotune::AutoTuneSpec spec)`
    - `.autotune(std::vector<int> block_sizes, std::vector<int> grid_wave_multipliers = {1})`
    - `.run()` -> `PipelineReport`
  - Fluent API `Suite`:
    - `.device(int device_id)`
    - `.correctness()`
    - `.benchmark(benchmark::BenchmarkConfig config = {})`
    - `.autotune(autotune::AutoTuneSpec spec)`
    - `.autotune(std::vector<int> block_sizes, std::vector<int> grid_wave_multipliers = {1})`
    - `.add(KernelDescriptor descriptor)`
    - `.run_all()` -> `SuiteReport`
  - `PipelineReport`:
    - хранит имя ядра, device_id, флаги включённых этапов, результат correctness, `std::optional<benchmark::BenchmarkResult>`, `std::optional<autotune::AutoTuneResult>`
    - `passed()` — true, если correctness не включён или прошёл успешно
    - `to_json(path)` / `to_csv(path)` — собственный экспорт pipeline-отчёта
  - `SuiteReport`:
    - хранит имя suite и `std::vector<PipelineReport>`
    - `all_passed()`
    - `to_json(dir)` / `to_csv(dir)` — по одному файлу на kernel report
  - Host-only unit tests для report/suite semantics, no-stage pipeline, duplicate-name protection, empty suite и файлового экспорта
  - CUDA integration tests для stage orchestration correctness -> benchmark -> autotune с реальным ядром

- Out of scope:
  - `.diagnose()` и rule-based рекомендации (задача `diagnostics-advisor`)
  - HTML export (задача `html-export`)
  - Параллельный прогон suite
  - Кэширование host/device buffer'ов между запусками pipeline
  - Единый cross-module export API для `BenchmarkResult`/`PipelineReport` в модуле `reporting` (это задача `export-refactor`)

## Affected Areas

- Modules:
  - `pipeline` (расширение после `kernel-descriptor`)
- Public headers:
  - `include/cuda_test/pipeline/pipeline_report.hpp`
  - `include/cuda_test/pipeline/pipeline.hpp`
  - `include/cuda_test/pipeline/suite.hpp`
  - `include/cuda_test/cuda_test.hpp` — umbrella include
- Existing modules used as dependencies:
  - `pipeline/kernel_descriptor.hpp`
  - `benchmark/benchmark_runner.hpp`
  - `autotune/search.hpp`
  - `core/device_info.hpp`
- Tests:
  - `tests/unit/pipeline/pipeline_test.cpp`
  - `tests/integration/pipeline/pipeline_cuda_test.cu`

## Interface Notes

### New Types

```cpp
namespace cuda_test::pipeline {

class PipelineReport {
public:
    [[nodiscard]] const std::string& kernel_name() const noexcept;
    [[nodiscard]] int device_id() const noexcept;
    [[nodiscard]] bool correctness_enabled() const noexcept;
    [[nodiscard]] bool correctness_passed() const noexcept;
    [[nodiscard]] bool benchmark_enabled() const noexcept;
    [[nodiscard]] bool autotune_enabled() const noexcept;
    [[nodiscard]] bool passed() const noexcept;

    [[nodiscard]] const std::optional<benchmark::BenchmarkResult>& benchmark_result() const noexcept;
    [[nodiscard]] const std::optional<autotune::AutoTuneResult>& autotune_result() const noexcept;

    void to_csv(const std::filesystem::path& path) const;
    void to_json(const std::filesystem::path& path) const;
};

class Pipeline {
public:
    explicit Pipeline(KernelDescriptor descriptor);

    Pipeline& device(int device_id);
    Pipeline& correctness();
    Pipeline& benchmark(benchmark::BenchmarkConfig config = {});
    Pipeline& autotune(autotune::AutoTuneSpec spec);
    Pipeline& autotune(std::vector<int> block_sizes,
                       std::vector<int> grid_wave_multipliers = {1});

    [[nodiscard]] PipelineReport run() const;
};

[[nodiscard]] Pipeline make_pipeline(KernelDescriptor descriptor);

class SuiteReport {
public:
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<PipelineReport>& reports() const noexcept;
    [[nodiscard]] bool all_passed() const noexcept;

    void to_csv(const std::filesystem::path& directory) const;
    void to_json(const std::filesystem::path& directory) const;
};

class Suite {
public:
    explicit Suite(std::string name);

    Suite& device(int device_id);
    Suite& correctness();
    Suite& benchmark(benchmark::BenchmarkConfig config = {});
    Suite& autotune(autotune::AutoTuneSpec spec);
    Suite& autotune(std::vector<int> block_sizes,
                    std::vector<int> grid_wave_multipliers = {1});
    Suite& add(KernelDescriptor descriptor);

    [[nodiscard]] SuiteReport run_all() const;
};

[[nodiscard]] Suite make_suite(std::string name);

} // namespace cuda_test::pipeline
```

### Pipeline Semantics

`Pipeline::run()` выполняет этапы в фиксированном порядке:

1. `correctness` — если включён, вызывает `descriptor.validate(descriptor.baseline_config(device_id))`
2. Если correctness включён и провален, pipeline завершает прогон сразу и не запускает performance-этапы
3. `benchmark` — если включён, запускает `BenchmarkRunner(config).run([&] { return descriptor.measure(baseline_config); })`
4. `autotune` — если включён, запускает `tune_kernel(spec, descriptor.problem_size(), measure_fn, validate_fn)`

`benchmark` измеряет baseline-конфигурацию. `autotune` ищет лучший launch-конфиг и не заменяет baseline benchmark, если оба этапа включены.

### Suite Semantics

`Suite` хранит общий pipeline-профиль. Для каждого descriptor в `run_all()` создаётся отдельный `Pipeline` с теми же настройками `device/correctness/benchmark/autotune`, затем выполняется `.run()`.

`Suite::add()` требует уникальные `descriptor.name()`, иначе бросает исключение. Это защищает от перезаписи файлов при `SuiteReport::to_json()` / `to_csv()`.

### Export Semantics

- `PipelineReport::to_json()` и `to_csv()` сериализуют сам pipeline-report, а не делегируют текущему `reporting::export_*`, потому что модуль `reporting` пока умеет экспортировать только `AutoTuneResult`
- JSON содержит summary pipeline + вложенные `benchmark` / `autotune` структуры, если они присутствуют
- CSV — компактный summary-формат с одной строкой на report
- `SuiteReport::to_json(dir)` / `to_csv(dir)` создают по одному файлу на kernel report с именами `<suite-name>_<kernel-name>.json|csv`
- Для удобства в корневом namespace `cuda_test` допускаются forwarder'ы `make_pipeline(...)` / `make_suite(...)`, но не `pipeline(...)`, потому что это имя занято namespace `cuda_test::pipeline`

## Risks

- **Pipeline/Suite добавляют второй публичный слой API.** Есть риск дублировать низкоуровневые возможности `BenchmarkRunner`/`tune_kernel`. Mitigation: `Pipeline` только оркестрирует существующие компоненты, не расширяя их семантику.
- **Экспорт в `reporting` ещё не унифицирован.** Mitigation: временно реализовать экспорт в `pipeline_report.hpp`, а `export-refactor` позже сведёт форматы.
- **Повторные `validate()` внутри autotune дорогие.** `tune_kernel` вызывает validator на каждом кандидате, а `KernelDescriptor` не кэширует буферы. Mitigation: принять overhead в этой задаче и зафиксировать как follow-up для будущего caching-слоя.
- **Suite применяет один stage-profile ко всем descriptor'ам.** Это ограничивает пер-kernel настройку. Mitigation: это осознанное упрощение первой итерации orchestration API.

## Acceptance Criteria

- [ ] `Pipeline(descriptor).correctness().run()` выполняет validate и возвращает report с `correctness_passed`
- [ ] `Pipeline` при включённом correctness и провале валидации не выполняет benchmark/autotune
- [ ] `Pipeline(descriptor).benchmark().run()` возвращает report с заполненным `benchmark_result`
- [ ] `Pipeline(descriptor).autotune(spec).run()` возвращает report с заполненным `autotune_result`
- [ ] Полная цепочка `.correctness().benchmark(...).autotune(...)` работает в одном `.run()`
- [ ] `Pipeline::device(device_id)` применяется к baseline validate/benchmark и к autotune spec
- [ ] `PipelineReport::to_json()` создаёт валидный JSON-файл
- [ ] `PipelineReport::to_csv()` создаёт CSV summary-файл
- [ ] `Suite` применяет общий stage-profile ко всем добавленным descriptor'ам
- [ ] `Suite::add()` бросает исключение при дублировании имён
- [ ] `Suite::run_all()` возвращает report на каждый descriptor
- [ ] `SuiteReport::all_passed()` возвращает `false`, если хотя бы один report не passed
- [ ] `SuiteReport::to_json(dir)` и `to_csv(dir)` создают по одному файлу на kernel
- [ ] Host-only unit tests покрывают report/suite semantics, duplicate names, empty suite и export helpers
- [ ] CUDA integration tests подтверждают полный cycle correctness -> benchmark -> autotune с реальным ядром

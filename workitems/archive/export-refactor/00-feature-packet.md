# Feature Packet

## Task

- Name: Post-MVP Phase 3 — Export Refactor
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/export-refactor`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` — решает проблему P3

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §10 (формат отчётов: CSV/JSON), §19 (экспериментальные таблицы)
- Related section(s) in vision: «P3 — Несогласованный стиль API»

## Objective

Рефакторинг модуля `reporting` для поддержки экспорта не только `AutoTuneResult`, но и `BenchmarkResult` и `PipelineReport`. Привести export API к единому стилю: перегрузки `export_csv` / `export_json` для каждого типа результата, а существующие `PipelineReport::to_csv()` / `to_json()` превратить в тонкие делегаты поверх `reporting`.

## Scope

- In scope:
  - Перегрузки `export_csv(path, BenchmarkResult)` и `export_json(path, BenchmarkResult)`
  - Перегрузки `export_csv(path, PipelineReport)` и `export_json(path, PipelineReport)` — экспорт составного отчёта (корректность + bench + tune)
  - Удаление дублирующей serialization-логики из `pipeline_report.hpp`; `PipelineReport::to_csv()` / `to_json()` делегируют в `reporting::export_*`
  - JSON для `BenchmarkResult`: объект с `sample_count`, optional `samples`, per-stage stats (h2d, kernel, d2h, total)
  - CSV для `BenchmarkResult`: одна строка summary со средними/медианами по этапам
  - JSON для `PipelineReport`: envelope с `kernel_name`, `correctness`, вложенными `benchmark` и `autotune` секциями
  - Сохранение обратной совместимости: существующие `export_csv(path, AutoTuneResult)` и `export_json(path, AutoTuneResult)` остаются без изменений
  - unit-тесты для новых перегрузок и обратной совместимости

- Out of scope:
  - HTML-экспорт (задача html-export)
  - Изменение формата существующих CSV/JSON для AutoTuneResult
  - Экспорт SuiteReport (делегируется в Suite через цикл по PipelineReport)

## Affected Areas

- Modules: `reporting`
- Public headers:
  - `include/cuda_test/reporting/export.hpp` — добавление перегрузок
- Related headers:
  - `include/cuda_test/pipeline/pipeline_report.hpp` — removal of duplicate formatting code and delegation to reporting
- Dependencies:
  - `benchmark::BenchmarkResult` — существующий тип
  - `pipeline::PipelineReport` — из задачи pipeline-suite
- Internal components:
  - `src/reporting/` — при наличии `.cpp` файлов

## Interface Notes

### Новые перегрузки

```cpp
namespace cuda_test::reporting {

// Существующие (без изменений)
void export_csv(const std::filesystem::path& path, const autotune::AutoTuneResult& result);
void export_json(const std::filesystem::path& path, const autotune::AutoTuneResult& result);

// Новые
void export_csv(const std::filesystem::path& path, const benchmark::BenchmarkResult& result);
void export_json(const std::filesystem::path& path, const benchmark::BenchmarkResult& result);

void export_csv(const std::filesystem::path& path, const pipeline::PipelineReport& report);
void export_json(const std::filesystem::path& path, const pipeline::PipelineReport& report);

} // namespace cuda_test::reporting
```

### Формат JSON для BenchmarkResult

```json
{
  "sample_count": 30,
  "h2d_stats": { "mean_ms": ..., "median_ms": ..., "p95_ms": ..., "ci95_low": ..., "ci95_high": ..., "cv": ... },
  "kernel_stats": { ... },
  "d2h_stats": { ... },
  "total_stats": { ... }
}
```

`BenchmarkResult` не содержит launch config, поэтому export для него не включает `config`.

### Формат JSON для PipelineReport

```json
{
  "kernel_name": "density_update",
  "device_id": 0,
  "correctness": { "enabled": true, "passed": true },
  "benchmark": { ... },
  "autotune": { ... }
}

```

Секции `benchmark` и `autotune` включаются только если соответствующий этап был запущен. Формат вложенных объектов соответствует `BenchmarkResult` JSON и `AutoTuneResult` JSON.

### Формат CSV для BenchmarkResult

```
h2d_mean_ms,h2d_median_ms,h2d_p95_ms,h2d_ci95_low,h2d_ci95_high,h2d_cv,kernel_mean_ms,...,total_cv
<values>
```

Одна строка данных (summary). Header row + data row.

### Формат CSV для PipelineReport

```
kernel_name,device_id,correctness_enabled,correctness_passed,passed,...,autotune_reason
<values>
```

Одна строка, объединяющая ключевые поля из bench и tune. Если этап не выполнялся — пустые ячейки.

## Risks

- **Зависимость от pipeline-suite.** `PipelineReport` определяется в задаче pipeline-suite. Mitigation: export-refactor реализуется после pipeline-suite; перегрузки для `BenchmarkResult` можно реализовать параллельно.
- **Формат CSV для PipelineReport может быть перегружен.** Одна строка с 20+ колонками. Mitigation: это summary-формат; детальные данные доступны через JSON.
- **Header-only cross-module wiring.** `pipeline_report.hpp` не может просто включить `reporting/export.hpp` из-за циклической зависимости. Mitigation: использовать forward declaration export-функций и inline delegation.

## Acceptance Criteria

- [ ] `export_json(path, BenchmarkResult)` создаёт валидный JSON с per-stage stats
- [ ] `export_csv(path, BenchmarkResult)` создаёт CSV с header + 1 data row
- [ ] `export_json(path, PipelineReport)` создаёт JSON с conditional sections (benchmark/autotune только если включены)
- [ ] `export_csv(path, PipelineReport)` создаёт CSV summary row
- [ ] `PipelineReport::to_json()` / `to_csv()` продолжают работать, но через `reporting::export_*`
- [ ] Существующие `export_*(path, AutoTuneResult)` работают без изменений (обратная совместимость)
- [ ] Parent directories создаются автоматически (как в текущей реализации)
- [ ] unit-тесты проверяют каждую перегрузку с минимальным валидным входом

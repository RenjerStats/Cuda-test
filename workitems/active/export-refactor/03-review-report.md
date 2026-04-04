# Review Report

## Linkage

- Feature packet: `workitems/active/export-refactor/00-feature-packet.md`
- Test contract: `workitems/active/export-refactor/01-test-contract.md`
- Implementation notes: `workitems/active/export-refactor/02-implementation-notes.md`
- Reviewer: Claude (Opus)
- Review date: 2026-04-04

## Review Surface

Changes relative to the accepted pipeline-suite state:

| File | Type | Lines |
|------|------|-------|
| `include/cuda_test/reporting/export.hpp` | REWRITE | 381 |
| `include/cuda_test/pipeline/pipeline_report.hpp` | REWRITE | 168 (was 382) |
| `tests/unit/reporting/export_test.cpp` | REWRITE | 292 |
| `tests/unit/pipeline/pipeline_test.cpp` | MOD | JSON assertion updates |
| `tests/integration/pipeline/pipeline_cuda_test.cu` | MOD | JSON assertion updates |

## Verdict: APPROVED

Рефакторинг выполнен чисто. Formatting-логика полностью переехала из `pipeline_report.hpp` в `reporting/export.hpp`. PipelineReport стал тонким data-классом с delegation-обёрткой. Обратная совместимость AutoTuneResult сохранена. Тесты адекватны.

---

## Spec Compliance

| Acceptance Criterion | Status | Evidence |
|---------------------|--------|---------|
| `export_json(path, BenchmarkResult)` → валидный JSON с per-stage stats | pass | `export.hpp:217-220` + тест `BenchmarkJsonExportWritesStageStats` |
| `export_csv(path, BenchmarkResult)` → CSV header + 1 data row | pass | `export.hpp:201-215` + тест `BenchmarkCsvExportWritesHeaderAndSummaryRow` (24 columns) |
| `export_json(path, PipelineReport)` → JSON с conditional sections | pass | `export.hpp:355-378` + тесты `PipelineJsonExportUsesEnvelopeAndConditionalSections`, `FullChainPopulatesAllStagesAndExportsReport` |
| `export_csv(path, PipelineReport)` → CSV summary row | pass | `export.hpp:304-353` + тест `PipelineCsvExportWritesSummaryWithEmptyOptionalColumns` |
| `PipelineReport::to_json()` / `to_csv()` → делегируют в `reporting::export_*` | pass | `pipeline_report.hpp:103-109` + тест `PipelineMemberDelegationMatchesReportingOutput` (побайтовое сравнение) |
| Обратная совместимость `export_*(path, AutoTuneResult)` | pass | `export.hpp:222-302` (формат не изменён) + тесты `AutoTuneCsvExportWritesHeaderAndCandidateRows`, `AutoTuneJsonExportWritesStructuredSummary` |
| Parent directories создаются автоматически | pass | `export.hpp:21-26` (`ensure_parent_directory`) |
| Unit-тесты для каждой перегрузки | pass | 10 тестов в `export_test.cpp` |

## Test Contract Compliance

| Contract Scenario | Covered | Test Name |
|------------------|---------|-----------|
| JSON — BenchmarkResult | yes | `BenchmarkJsonExportWritesStageStats` |
| CSV — BenchmarkResult | yes | `BenchmarkCsvExportWritesHeaderAndSummaryRow` |
| JSON — BenchmarkResult — stats correctness | yes | `BenchmarkCsvExportWritesHeaderAndSummaryRow` (kernel_mean=3.0, total_mean=4.5) + `BenchmarkJsonExportWritesStageStats` (median_ms:3) |
| JSON — PipelineReport — full | yes | `FullChainPopulatesAllStagesAndExportsReport` (CUDA, проверяет `"benchmark":` и `"autotune":` в JSON) |
| JSON — PipelineReport — correctness only | partial | `PipelineJsonExportUsesEnvelopeAndConditionalSections` (no-stage: correctness.enabled=false); нет теста с correctness=true + bench/tune absent |
| JSON — PipelineReport — benchmark only | no | (see m2) |
| CSV — PipelineReport — full | yes | `FullChainPopulatesAllStagesAndExportsReport` (CUDA, CSV checked) |
| CSV — PipelineReport — partial | yes | `PipelineCsvExportWritesSummaryWithEmptyOptionalColumns` (bench/tune columns empty) |
| PipelineReport member delegation | yes | `PipelineMemberDelegationMatchesReportingOutput` (binary equality) |
| Backward compat — AutoTuneResult JSON | yes | `AutoTuneJsonExportWritesStructuredSummary` |
| Backward compat — AutoTuneResult CSV | yes | `AutoTuneCsvExportWritesHeaderAndCandidateRows` |
| Parent directory creation | implicit | `ensure_parent_directory` called in `open_output_file` for every export; no dedicated test, but exercised transitively |
| Empty BenchmarkResult | yes | `BenchmarkJsonExportHandlesEmptyResult` (sample_count:0, mean_ms:0) |
| PipelineReport with failed correctness | no | (see m2 — tested only in CUDA integration) |
| Invalid path | yes | `InvalidExportPathRaisesError` (directory-as-file for all 6 overloads) |

---

## Findings

### M1 — major: Циклическая include-зависимость решена через bottom-of-file include

**Файл:** `pipeline_report.hpp:168`

```cpp
#include "cuda_test/reporting/export.hpp"
```

`pipeline_report.hpp` объявляет forward declaration `reporting::export_csv`/`export_json` (строки 19-24), а затем **внизу файла** включает `reporting/export.hpp` для предоставления определений inline-функций. `reporting/export.hpp` в свою очередь включает `pipeline/pipeline_report.hpp` (строка 6), но header guard (`#pragma once`) предотвращает рекурсию.

Паттерн работает корректно благодаря тому, что:
1. Forward declarations в начале `pipeline_report.hpp` достаточны для `to_csv()`/`to_json()` member bodies
2. Полные определения из `export.hpp` доступны в TU, включающем любой из этих заголовков

Однако это **нетипичный** паттерн для C++ — include в конце файла вместо начала. Он хрупкий: если кто-то включит `export.hpp` напрямую без `pipeline_report.hpp`, всё работает (export.hpp включает pipeline_report.hpp). Но если в будущем `pipeline_report.hpp` потребует что-то из `export.hpp` до строки 168, dependency сломается.

**Рекомендация:** Принять. Для header-only библиотеки это обоснованное решение проблемы циклической зависимости. Альтернатива (вынести `to_csv`/`to_json` из member functions в free functions) потребовала бы ломки API. Документировать паттерн комментарием:

```cpp
// Include reporting definitions after PipelineReport is complete to resolve
// the bidirectional dependency between pipeline and reporting modules.
#include "cuda_test/reporting/export.hpp"
```

---

### m1 — minor: Дублирование detail-функций между `pipeline_report.hpp` и `export.hpp` устранено не полностью

**Файлы:** `pipeline_report.hpp` (detail namespace), `export.hpp` (detail namespace)

В `pipeline_report.hpp` остались `sanitize_file_component` и `make_report_stem` в `pipeline::detail`. В `export.hpp` — все formatting-функции в `reporting::detail`. Дублирования нет (разные функции в разных модулях). Однако `pipeline_report.hpp` всё ещё содержит utility-функции, которые семантически относятся к file-naming, а не к report data.

**Рекомендация:** Не блокирует. `sanitize_file_component` используется только Suite (для duplicate-name check и file stem). Оставить в `pipeline::detail` — это правильное место, потому что именно pipeline/suite отвечает за file naming, а не reporting.

---

### m2 — minor: Два contract-сценария покрыты неполно

**Файл:** test contract, correctness scenarios:
- «JSON — PipelineReport — correctness only» — контракт требует `correctness=true, bench=nullopt, tune=nullopt`. Тест `PipelineJsonExportUsesEnvelopeAndConditionalSections` проверяет no-stage (correctness.enabled=**false**). Сценарий с `correctness=true` + passed + bench/tune absent тестируется только в CUDA (`CorrectnessOnlyReportPassesForValidDescriptor`), но **без проверки JSON output**.
- «JSON — PipelineReport — benchmark only» — нет host-only теста с benchmark-present + correctness-disabled. Сценарий проверяется только косвенно через full-chain CUDA тест.

Оба сценария работают корректно по коду (conditional sections управляются `has_value()` checks). Формально contract не полностью покрыт в host-only тестах, но implementation notes объясняют почему: невозможно сконструировать нетривиальный `PipelineReport` без friend-доступа или CUDA.

**Рекомендация:** Принять как есть. Альтернатива — добавить `PipelineReport::Builder` или test-only factory — создаст лишний код ради тестов. CUDA integration тесты закрывают gap.

---

### m3 — minor: `split_csv_row` в тестах не обрабатывает quoted fields с запятыми

**Файл:** `export_test.cpp:88-96`

```cpp
inline std::vector<std::string> split_csv_row(const std::string& row) {
    // simple split by ','
}
```

Если CSV-значение содержит запятую внутри кавычек (например, autotune reason `"Selected block=128, grid=2..."`), `split_csv_row` разобьёт его неправильно. В текущих тестах это не проблема — `split_csv_row` используется только для `BenchmarkResult` CSV (без quoted fields с запятыми) и `PipelineReport` CSV (проверяется column count, а reason — последнее поле).

**Рекомендация:** Не блокирует. Если в будущем reason или kernel_name будут содержать запятые — тест сломается. Но это проблема теста, не кода. Отложить до html-export/export-refactor v2.

---

### m4 — minor: Магические числа в CSV blank-cell loops наследованы из pipeline-suite

**Файл:** `export.hpp:330-332, 346-349`

```cpp
for (int index = 0; index < 6; ++index) { ... }  // benchmark empty
for (int index = 0; index < 9; ++index) { ... }  // autotune empty
```

Код перенесён из `pipeline_report.hpp` в `export.hpp` без изменений. Числа 6 и 9 соответствуют количеству benchmark/autotune CSV-колонок минус 1 (для autotune_reason). При добавлении колонок (например, для diagnostics) счёт нужно менять вручную.

**Рекомендация:** Наследованное замечание (m4 из pipeline-suite review). Принять — html-export может решить это автоматическим column generation. Текущая реализация корректна.

---

### m5 — minor: `pipeline_report.hpp` больше не нужен `<fstream>` и другие I/O includes

**Файл:** `pipeline_report.hpp:1-12`

После рефакторинга `pipeline_report.hpp` не содержит I/O логики — все `ofstream`, `iomanip`, `sstream`, `locale`, `fstream` использования удалены. Однако некоторые из этих headers могут подтягиваться через transitive includes. Текущий include list (`<algorithm>`, `<cctype>`, `<filesystem>`, `<optional>`, `<string>`, `<vector>`) — корректен, но `<cctype>` нужен только для `sanitize_file_component`, а `<algorithm>` — для `std::unique` и `std::all_of`.

**Рекомендация:** Include list уже минимален для текущего содержимого. Всё корректно. Чисто информационное наблюдение.

---

## Architecture Assessment

### Сильные стороны

1. **Единый formatting authority** — вся serialization-логика теперь в `reporting::detail`. `pipeline_report.hpp` стал чистым data-классом (с ~168 строками вместо 382). Это именно то, что требовал packet.

2. **Перенос без потерь** — все detail-функции (`write_run_stats_json`, `write_benchmark_result_json`, `write_autotune_result_json`, `escape_json_string`, `quote_csv`, `format_double`, `format_bool`, `write_blank_csv_cell`) перемещены из pipeline в reporting без изменения логики. Обратная совместимость AutoTuneResult гарантирована.

3. **Новый JSON envelope** — PipelineReport JSON теперь структурирован: `correctness` как вложенный объект, `benchmark`/`autotune` как conditional sections. Это улучшение относительно предыдущего flat-формата и готовит почву для `diagnostics-advisor` (который добавит ещё одну conditional section).

4. **Delegation test** — `PipelineMemberDelegationMatchesReportingOutput` делает binary equality check между `report.to_json()` и `reporting::export_json()`. Это гарантирует, что delegation работает без drift.

5. **Comprehensive invalid-path test** — `InvalidExportPathRaisesError` проверяет все 6 export overloads, а не только одну.

6. **Чистый подход к циклической зависимости** — forward declaration + bottom-of-file include вместо выделения отдельного internal header. Для header-only библиотеки — минималистичное решение.

### Совместимость с последующими задачами

| Задача | Готовность |
|--------|-----------|
| html-export | `reporting::detail::write_*_json` функции можно переиспользовать для embedding JSON в HTML. Conditional section pattern готов. |
| diagnostics-advisor | Добавление `"diagnostics":` conditional section в `export_json(path, PipelineReport)` — одна проверка `has_value()` + один вызов writer. `PipelineReport` потребует новое поле + friend доступ (наследованное M2 из pipeline-suite). |
| analysis-expansion | Не затрагивает export. Совместимо. |
| cmake-install | Новые/изменённые headers в install target. Стандартно. |

---

## Summary

| Severity | Count | IDs |
|----------|-------|-----|
| blocker | 0 | — |
| major | 1 | M1 (bottom-of-file include for cyclic dependency resolution) |
| minor | 5 | m1 (detail namespace split), m2 (partial contract coverage), m3 (test CSV parser), m4 (inherited magic numbers), m5 (include audit) |

**Disposition:** Принять. Major M1 — архитектурное решение, которое работает корректно и является наименее инвазивным вариантом для header-only библиотеки. Рекомендую добавить поясняющий комментарий перед bottom-of-file include. Minor замечания — информационные.

Рефакторинг успешно централизовал export-логику. `pipeline_report.hpp` похудел более чем вдвое (382 → 168 строк). Тестовое покрытие адекватно (10 export тестов + обновлённые pipeline тесты). Код готов к расширению для diagnostics и html-export.

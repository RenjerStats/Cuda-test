# Review Report

## Linkage

- Feature packet: `workitems/active/pipeline-suite/00-feature-packet.md`
- Test contract: `workitems/active/pipeline-suite/01-test-contract.md`
- Implementation notes: `workitems/active/pipeline-suite/02-implementation-notes.md`
- Reviewer: Claude (Opus)
- Review date: 2026-04-04

## Review Surface

Uncommitted changes on branch `task/kernel-descriptor` (pipeline-suite delivered on top of accepted kernel-descriptor) relative to the kernel-descriptor acceptance point:

| File | Type | Lines |
|------|------|-------|
| `include/cuda_test/pipeline/pipeline_report.hpp` | NEW | 382 |
| `include/cuda_test/pipeline/pipeline.hpp` | NEW | 117 |
| `include/cuda_test/pipeline/suite.hpp` | NEW | 119 |
| `include/cuda_test/cuda_test.hpp` | MOD | +3 (pipeline includes) |
| `tests/CMakeLists.txt` | MOD | +2 (test targets) |
| `tests/unit/pipeline/pipeline_test.cpp` | NEW | 143 |
| `tests/integration/pipeline/pipeline_cuda_test.cu` | NEW | 276 |

## Verdict: APPROVED

Реализация чистая и компактная. Pipeline корректно оркестрирует три этапа, Suite адекватно дублирует pipeline-профиль, экспорт работает. Все acceptance criteria выполнены. Найденные замечания — не блокеры; два major-замечания фиксируют архитектурные решения, которые необходимо учесть при последующих задачах.

---

## Spec Compliance

| Acceptance Criterion | Status | Evidence |
|---------------------|--------|---------|
| `Pipeline(descriptor).correctness().run()` → report с `correctness_passed` | pass | `pipeline.hpp:65-70` + тест `CorrectnessOnlyReportPassesForValidDescriptor` |
| Correctness провал → benchmark/autotune не выполняются | pass | `pipeline.hpp:67-69` (early return) + тест `FailingCorrectnessSkipsBenchmarkAndAutotune` |
| `Pipeline(descriptor).benchmark().run()` → report с `benchmark_result` | pass | `pipeline.hpp:72-77` + тест `BenchmarkOnlyUsesBaselineConfigurationAndProducesStats` |
| `Pipeline(descriptor).autotune(spec).run()` → report с `autotune_result` | pass | `pipeline.hpp:79-88` + тест `AutotuneOnlyReturnsAllCandidatesAndPropagatesDevice` |
| Полная цепочка `.correctness().benchmark().autotune()` → один `.run()` | pass | тест `FullChainPopulatesAllStagesAndExportsReport` |
| `Pipeline::device(device_id)` применяется к baseline и autotune spec | pass | `pipeline.hpp:81` (`spec.device_id = device_id_`) + тест `AutotuneOnlyReturnsAllCandidatesAndPropagatesDevice` (проверяет `best.device_id` и каждый candidate) |
| `PipelineReport::to_json()` создаёт валидный JSON | pass | `pipeline_report.hpp:300-325` + тесты `PipelineReportExportsJsonAndCsvForNoStageRun`, `FullChainPopulatesAllStagesAndExportsReport` |
| `PipelineReport::to_csv()` создаёт CSV summary | pass | `pipeline_report.hpp:251-298` + тесты выше |
| Suite применяет общий stage-profile ко всем descriptors | pass | `suite.hpp:69-89` (run_all строит Pipeline с теми же настройками) + тест `SuiteAppliesConfiguredStagesToAllDescriptors` |
| `Suite::add()` бросает исключение при дублировании имён | pass | `suite.hpp:50-63` + тест `SuiteRejectsDuplicateAndExportCollidingNames` |
| `Suite::run_all()` → report на каждый descriptor | pass | `suite.hpp:69-89` + тесты `SuiteAppliesConfiguredStagesToAllDescriptors`, `SuiteReportsPartialFailureAndWritesFiles` |
| `SuiteReport::all_passed()` → false если хотя бы один не passed | pass | `pipeline_report.hpp:351-354` + тест `SuiteReportsPartialFailureAndWritesFiles` |
| `SuiteReport::to_json(dir)` / `to_csv(dir)` → по файлу на kernel | pass | `pipeline_report.hpp:357-373` + тесты `SuiteWritesOneFilePerKernelForNoStageReports`, `SuiteReportsPartialFailureAndWritesFiles` |
| Host-only unit tests для report/suite semantics | pass | 6 тестов в `pipeline_test.cpp` |
| CUDA integration tests для stage orchestration | pass | 7 тестов в `pipeline_cuda_test.cu` |

## Test Contract Compliance

| Contract Scenario | Covered | Test Name |
|------------------|---------|-----------|
| Pipeline — no stages | yes | `NoStagePipelineReturnsVacuousSuccess` |
| Pipeline — correctness only (CUDA) | yes | `CorrectnessOnlyReportPassesForValidDescriptor` |
| Pipeline — correctness fails (CUDA) | yes | `FailingCorrectnessSkipsBenchmarkAndAutotune` |
| Pipeline — benchmark only (CUDA) | yes | `BenchmarkOnlyUsesBaselineConfigurationAndProducesStats` |
| Pipeline — autotune only (CUDA) | yes | `AutotuneOnlyReturnsAllCandidatesAndPropagatesDevice` |
| Pipeline — full chain (CUDA) | yes | `FullChainPopulatesAllStagesAndExportsReport` |
| Pipeline — device selection (CUDA) | yes | `DeviceSelectionIsReflectedInNoStageReport` (host) + `AutotuneOnlyReturnsAllCandidatesAndPropagatesDevice` (CUDA, verifies candidate propagation) |
| Pipeline — benchmark baseline config (CUDA) | yes | `BenchmarkOnlyUsesBaselineConfigurationAndProducesStats` (descriptor asserts block.x=128, grid.x=8 inside launch) |
| Pipeline — autotune config propagation (CUDA) | yes | `AutotuneOnlyReturnsAllCandidatesAndPropagatesDevice` (spec {64,128}×{1,2} → 4 candidates, device_id checked per candidate) |
| PipelineReport::to_json | yes | `PipelineReportExportsJsonAndCsvForNoStageRun` + `FullChainPopulatesAllStagesAndExportsReport` |
| PipelineReport::to_csv | yes | `PipelineReportExportsJsonAndCsvForNoStageRun` + `FullChainPopulatesAllStagesAndExportsReport` |
| Suite — empty | yes | `EmptySuiteReturnsSuccessAndEmitsNoFiles` |
| Suite — common stage profile | yes | `SuiteAppliesConfiguredStagesToAllDescriptors` |
| Suite — duplicate name rejection | yes | `SuiteRejectsDuplicateAndExportCollidingNames` (also tests export-safe collision) |
| Suite — partial failure | yes | `SuiteReportsPartialFailureAndWritesFiles` |
| SuiteReport::to_json(dir) | yes | `SuiteWritesOneFilePerKernelForNoStageReports` + `SuiteReportsPartialFailureAndWritesFiles` |
| SuiteReport::to_csv(dir) | yes | `SuiteWritesOneFilePerKernelForNoStageReports` + `SuiteReportsPartialFailureAndWritesFiles` |
| Empty autotune block list | no | (negative scenario — see m3) |
| Invalid benchmark config | no | (negative scenario — see m3) |
| Suite export to nested missing dir | yes | `ensure_parent_directory` + `create_directories` in SuiteReport::to_csv/to_json |
| Suite with empty name | partial | empty suite tested; empty name + add descriptor not explicitly tested, but logic allows it |

---

## Findings

### M1 — major: `Pipeline::run()` — `const` метод мутирует внутреннее состояние через descriptor

**Файл:** `pipeline.hpp:52`

```cpp
[[nodiscard]] PipelineReport run() const {
```

`Pipeline::run()` объявлен `const`, но при выполнении вызывает `descriptor_.validate()` и `descriptor_.measure()`, которые через `shared_ptr<const Concept>` вызывают `mutable` factory/launch на Model. Формально — корректно (logical const через type erasure + mutable). Однако это означает, что `run()` не thread-safe даже для разных Pipeline объектов, если они разделяют один descriptor (через copy).

Это наследованная проблема от M2 ревью kernel-descriptor. Здесь она становится более заметной, потому что `Suite::run_all()` строит Pipeline из `const KernelDescriptor&` (строка 74), и если бы Suite когда-нибудь стал параллельным, factory collision был бы неизбежен.

**Рекомендация:** Задокументировано на уровне descriptor (kernel-descriptor M2). На уровне Pipeline — принять как inherited constraint. При реализации параллельного Suite в будущем — каждый pipeline должен получать deep copy descriptor с независимыми factory instances.

---

### M2 — major: PipelineReport использует `friend class` для доступа к приватным полям

**Файл:** `pipeline_report.hpp:337-338`

```cpp
friend class Pipeline;
friend class SuiteReport;
```

`PipelineReport` не имеет публичных мутаторов — Pipeline заполняет его через friend-доступ к private-полям напрямую (`report.kernel_name_ = ...`, `report.correctness_passed_ = ...`). Аналогично SuiteReport заполняется через `friend class Suite`.

Это работает, но создаёт tight coupling: любое добавление поля в PipelineReport (например, `diagnostics_result_` для задачи diagnostics-advisor) потребует модификации и Pipeline, и SuiteReport, и самого PipelineReport.

**Альтернатива:** Builder или named-parameter struct для конструирования report. Однако для текущего scope (3 producers: Pipeline, Suite, diagnostics-advisor в будущем) friend class — приемлемое решение.

**Рекомендация:** Принять. При добавлении `diagnostics_advisor` — если friend-список растёт > 3 классов, рассмотреть рефакторинг в builder.

---

### m1 — minor: `BenchmarkConfig{1, 2}` — aggregate init with positional args

**Файл:** `pipeline_cuda_test.cu:134, 150, 218, 243`

```cpp
.benchmark({1, 2})
.benchmark({1, 3})
```

`BenchmarkConfig` — struct с `warmup_runs` и `measure_runs`. Aggregate initialization `{1, 2}` корректна, но при чтении тестов неочевидно, какое значение — warmup, а какое — measure. Если BenchmarkConfig получит третье поле, порядок может стать ещё более запутанным.

**Рекомендация:** Предпочесть named initialization:
```cpp
.benchmark({.warmup_runs = 1, .measure_runs = 2})
```
Designated initializers — C++20, но MSVC 2022 поддерживает их как extension даже в C++17 mode. Не блокирует — косметика.

---

### m2 — minor: Дублирование `ScopedTempDir` и `read_text_file` между unit и CUDA тестами

**Файлы:** `pipeline_test.cpp:15-40`, `pipeline_cuda_test.cu:27-52`

Идентичные utility-классы `ScopedTempDir` и `read_text_file` скопированы в оба тестовых файла. При изменении поведения (например, добавление cleanup-логики) нужно менять оба места.

**Рекомендация:** Вынести в `tests/fixtures/scoped_temp_dir.hpp`. Не блокирует — тесты корректны. Можно отложить до следующей задачи, если она затронет тесты.

---

### m3 — minor: Два негативных сценария из test contract не покрыты тестами

**Файл:** test contract, negative scenarios 1-2:
- «Pipeline with empty autotune block list» — spec требует `throws std::invalid_argument`. Не тестировано.
- «Pipeline with invalid benchmark config» — spec требует exception propagation. Не тестировано.

Оба сценария работают корректно по коду: пустой `block_sizes` бросит исключение из `tune_kernel`, а `BenchmarkConfig{-1, 0}` бросит из `BenchmarkRunner`. Но отсутствие тестов означает, что контракт не доказан.

**Рекомендация:** Добавить 2 host-only теста:
```cpp
TEST(PipelineSuiteTest, EmptyAutotuneBlockListThrows) {
    EXPECT_THROW(
        make_pipeline(make_no_stage_descriptor("neg_autotune"))
            .autotune({})
            .run(),
        std::invalid_argument);
}

TEST(PipelineSuiteTest, InvalidBenchmarkConfigPropagatesException) {
    EXPECT_THROW(
        make_pipeline(make_no_stage_descriptor("neg_benchmark"))
            .benchmark({-1, 0})
            .run(),
        std::invalid_argument);
}
```

Одна-две минуты работы. Рекомендую добавить в текущей ветке.

---

### m4 — minor: CSV пустые ячейки — `write_blank_csv_cell` не записывает разделитель для первого поля группы

**Файл:** `pipeline_report.hpp:276-279, 291-295`

```cpp
for (int index = 0; index < 6; ++index) {
    detail::write_blank_csv_cell(stream);
}
```

Каждый вызов `write_blank_csv_cell(stream)` с default `add_separator=true` выводит `,` — получается `,,,,,,`. Для benchmark это 6 запятых (6 пустых ячеек). Корректно: предыдущее поле (`autotune_enabled`) записано без trailing comma, а `write_blank_csv_cell` начинает с comma. Для autotune — 9 `write_blank_csv_cell` + `detail::quote_csv("")` = правильное число ячеек. Логика корректна, но хрупкая — при добавлении колонок легко сбить счёт магических чисел (6, 9).

**Рекомендация:** Задача `export-refactor` должна свести все CSV-экспорты в единый формат. Там же заменить магические числа на автоматический подсчёт колонок. Не требует исправления сейчас.

---

### m5 — minor: `autotune::AutoTuneSpec` копируется на каждый `run()` вызов

**Файл:** `pipeline.hpp:80`

```cpp
autotune::AutoTuneSpec spec = *autotune_spec_;
```

`AutoTuneSpec` содержит два `vector<int>`, что значит heap allocation при копировании. Для типичного use case (1-2 вызова run()) это незначительно. Но если descriptor reuse потребует повторных run() — можно передавать по const ref, модифицируя только `device_id` локально.

**Рекомендация:** Не блокирует. Текущий паттерн «copy + mutate device_id» — самый безопасный. Оптимизация не оправдана для текущего масштаба использования.

---

### m6 — minor: Suite::run_all() не проверяет, что device_id допустим

**Файл:** `suite.hpp:69-89`

Suite::run_all() передаёт device_id_ в Pipeline::device(), который проверяет только `device_id < 0`. Если device_id указывает на несуществующее устройство, ошибка возникнет только при вызове `descriptor.baseline_config(device_id)` или глубже в CUDA runtime. Ошибка поздняя и менее информативная.

**Рекомендация:** Не блокирует — текущее поведение корректно (ошибка всё равно всплывёт). При желании можно добавить ранний `core::device_count()` check в Pipeline::run(). Defer to diagnostics-advisor или pipeline-suite v2.

---

## Architecture Assessment

### Сильные стороны

1. **Чистая декомпозиция на три заголовка** — `pipeline_report.hpp` (данные + экспорт), `pipeline.hpp` (оркестрация), `suite.hpp` (batch). Каждый файл имеет одну зону ответственности. Include graph чистый: suite → pipeline → pipeline_report, без циклов.

2. **Pipeline как thin orchestrator** — Pipeline не дублирует логику BenchmarkRunner/tune_kernel, а делегирует им. Это означает, что bug-fixes или оптимизации в Layer 1-2 автоматически подтягиваются в Layer 3.

3. **Early return при провале correctness** — правильный short-circuit. Pipeline не тратит время на benchmark/autotune для некорректного ядра.

4. **Усиленная проверка дубликатов в Suite** — collision на уровне sanitized file stem (не только raw name) предотвращает перезапись файлов при экспорте. Хорошее проактивное решение.

5. **Собственный JSON/CSV экспорт** — не зависит от текущего модуля `reporting`, который знает только `AutoTuneResult`. Решение осознанно документировано как временное, до `export-refactor`.

6. **Полное покрытие host-only сценариев** — no-stage pipeline, device selection, duplicate names, empty suite, file emission — всё проверено без CUDA.

### Совместимость с последующими задачами

| Задача | Готовность |
|--------|-----------|
| export-refactor | PipelineReport/SuiteReport имеют to_json/to_csv. Export-refactor объединит их с reporting::export_*. Нужно будет отрефакторить detail-функции в pipeline_report.hpp в общий модуль. Конфликтов нет. |
| analysis-expansion | Pipeline не использует analysis-модуль. Расширение analysis не затронет pipeline. |
| diagnostics-advisor | Pipeline потребует новый этап `.diagnose()` и новое поле в PipelineReport (`std::optional<DiagnosticsResult>`). Friend-доступ (M2) потребует добавления friend class или рефакторинга. Реализуемо. |
| html-export | PipelineReport/SuiteReport — входные данные для HTML. Данные доступны через public accessors. Совместимо. |
| cmake-install | Новые заголовки в `include/cuda_test/pipeline/` — нужно добавить в install target. Стандартная операция. |

---

## Summary

| Severity | Count | IDs |
|----------|-------|-----|
| blocker | 0 | — |
| major | 2 | M1 (inherited thread-safety from descriptor), M2 (friend-class coupling for report construction) |
| minor | 6 | m1 (positional aggregate init), m2 (test utility duplication), m3 (missing negative tests), m4 (magic CSV column counts), m5 (AutoTuneSpec copy), m6 (late device validation) |

**Disposition:** Принять. Major замечания M1 (inherited) и M2 (friend coupling) — архитектурные, не требуют изменений в текущей задаче. Minor m3 (два недостающих негативных теста) рекомендую добавить в текущей ветке — это 10 строк кода и полное закрытие test contract. Остальные minor — информационные или deferred к export-refactor/diagnostics-advisor.

Реализация создаёт чистый оркестрационный слой, корректно делегирующий в Layer 1-2 компоненты. Тестовое покрытие адекватно (13 тестов: 6 host + 7 CUDA). Код готов к наращиванию этапами diagnostics и export.

## Disposition Update

- Applied in current branch:
  - `m3` closed by adding host-only tests `EmptyAutotuneBlockListThrows` and `InvalidBenchmarkConfigPropagatesException`
  - `m1` partially addressed in test readability by replacing positional `BenchmarkConfig{...}` calls in CUDA tests with explicit helper-built configs
- Deferred by design:
  - `M1` inherited descriptor thread-safety constraint
  - `M2` friend-based report construction
  - `m2`, `m4`, `m5`, `m6` remain non-blocking follow-ups for later tasks

Post-fix verification:
- `build/msvc/tests/Debug/pipeline_suite_test.exe` -> passed (8 tests)
- `build/msvc-cuda/tests/Debug/pipeline_suite_cuda_test.exe` -> passed (7 tests)

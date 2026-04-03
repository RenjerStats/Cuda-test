# Feature Packet

## Task

- Name: Phase 7 - Kernel Integration / kernel-integration
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/kernel-integration`
- Status: approved

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §4 (scope boundaries), §5 (functional results), §6 (30 measured runs and 95% CI), §14 (unit-test approach), §16 (six target kernel classes), §18 (acceptance), §19 (report tables)
- Related section(s) in `plan/Доработанная заявка на НИР.md`: задача 6 (demonstration on examples), agreed test application context from the course project

## Objective

Закрыть интеграционный этап MVP: собрать полный runnable path для шести representative CUDA kernels, прогнать их через correctness tests, staged profiling, autotune, и сформировать report artifacts. Поскольку исходники самого курсового проекта в текущем репозитории отсутствуют, эта фаза интегрирует шесть встроенных representative kernels, совпадающих по классам операций с §16 записки; single-GPU evidence собирается на единственном доступном NVIDIA GPU, а второй GPU explicitly фиксируется как deferred hardware constraint.

## Scope

- In scope:
  - six bundled representative kernels aligned with §16 categories:
    - density update
    - physics integration
    - contact / neighborhood flagging
    - active element compaction
    - buffer generation
    - interval intersection / selection
  - reusable kernel fixtures under `tests/fixtures/`
  - CUDA integration tests for correctness, smallest meaningful input, and launch handling
  - one CUDA runner under `benchmarks/kernels/` that performs autotune for all six kernels
  - CSV/JSON evidence under `reports/tables/` for the single available GPU
  - documentation of the missing second-GPU run as a deferred hardware limitation

- Out of scope:
  - wiring against the actual external course-project repository
  - two-GPU evidence collection in the current environment
  - Nsight/CUPTI profiling or low-level memory diagnostics
  - graphical figure generation from the produced reports
  - general-purpose application adapters beyond the bundled six-kernel suite

## Affected Areas

- Modules:
  - integration path across `core`, `testing`, `benchmark`, `autotune`, `profiling`, `analysis`, `reporting`
- Public headers:
  - none required beyond the already completed modules
- Internal components:
  - `examples/kernels/`
  - `tests/fixtures/`
  - `tests/integration/`
  - `benchmarks/kernels/`
  - `benchmarks/CMakeLists.txt`
  - `tests/CMakeLists.txt`
  - `reports/tables/`
- Reports or methodology:
  - exported evidence must stay aligned with `docs/methodology/benchmark-protocol.md` and `plan/Пояснительная записка.md` §19

## Interface Notes

- New or changed types:
  - internal kernel-suite fixtures and case runners only
- New or changed functions:
  - internal helpers to run validation and profiling for each bundled kernel
- Input or output assumptions:
  - all bundled kernels use deterministic fixed fixtures
  - autotune defaults remain `warmup_runs=5`, `measure_runs=30`
  - correctness is validated before accepting timing data for each candidate
  - reports are generated for one detected NVIDIA GPU in the current environment

## Risks

- Technical risks:
  - representative kernels are not the exact course-project source; mitigation: keep their names and behavior aligned with the six planned operation classes and document the limitation explicitly
  - compaction-style kernels are harder to validate than pure map kernels; mitigation: use deterministic host reference logic and small inspectable fixtures
- Measurement risks:
  - laptop GPU clocks may fluctuate; mitigation: keep warm-up + 30 measured runs and record 95% CI from the existing benchmark module
- Integration risks:
  - the NIR target mentions two GPUs, but the current laptop exposes one; mitigation: produce single-GPU evidence now and log second-GPU validation as deferred rather than claiming completion

## Acceptance Criteria

- [ ] `examples/kernels/` contains six bundled representative kernels aligned with the planned operation classes in §16
- [ ] `tests/integration/` validates correctness for all six kernels on the available CUDA GPU
- [ ] `benchmarks/kernels/` contains a runnable autotune/report generator for all six kernels
- [ ] CSV/JSON evidence for all six kernels is generated under `reports/tables/` for the single available GPU
- [ ] `msvc-cuda` configure/build and selected integration tests pass
- [ ] implementation notes explicitly record that two-GPU evidence is deferred because only one GPU is available in the current environment

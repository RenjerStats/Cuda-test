# Project: cuda_test

C++17/CUDA library for isolated kernel testing, benchmarking, autotuning, profiling, and reporting.

## My Role

I am the **reviewer** (see `agents/claude-reviewer.md`).
GPT is the **lead** (see `agents/gpt-lead.md`).

## Before Every Task

1. Read `AGENTS.md` for global rules, stage-gate workflow, and module boundaries.
2. Read the active work item packet under `workitems/active/<task-name>/`.
3. Read `plan/Пояснительная записка.md` when requirements context is needed.

## Review Protocol

- Review against the diff (`git diff main...task/<branch>`), not the full tree.
- Classify findings as `blocker`, `major`, or `minor`.
- Fill `workitems/active/<task-name>/03-review-report.md` with findings and decision.
- Do NOT approve work that skipped `spec → tests → implementation → review`.

## Code Conventions

- C++17, CUDA where applicable.
- Public headers: `include/cuda_test/<module>/`.
- Sources: `src/<module>/`.
- Formatting: `.clang-format` (LLVM-based, 4-space indent, 100 col).
- Build: CMake ≥ 3.26, presets in `CMakePresets.json`.
- Tests: Google Test under `tests/`.
- Benchmarks: Google Benchmark under `benchmarks/`.

## Local Build Environment

- Locally verified build flow: VS Code CMake Tools + MSVC from Visual Studio 2022 Community.
- Verified build directory: `build/msvc`.
- Verified CMake path used by VS Code: `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`.
- In a plain PowerShell session, `cmake`, `cl`, and `ninja` may be missing from `PATH`; prefer VS Code build tasks or a VS 2022 developer shell when building manually.

## Module Boundaries

`core` · `testing` · `benchmark` · `autotune` · `profiling` · `analysis` · `reporting`

Do not introduce new top-level modules without a documented decision in `docs/decisions/`.

## Commit Prefixes

`spec:` · `test:` · `impl:` · `review:`

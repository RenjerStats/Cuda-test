# Project Agents

This repository hosts a C++17/CUDA library for isolated CUDA kernel testing, benchmarking, autotuning, profiling, and reporting.

## Canonical Sources

Read these sources in order when requirements conflict:

1. The explanatory note in `plan/`
2. The NIR assignment note in `plan/`
3. The approved work item packet in `workitems/active/<task-name>/`

Treat `plan/` as the research source of truth. Treat git history as the implementation source of truth.

## Permanent Roles

- `GPT lead`: own task decomposition, specification, test contract, implementation, and self-check.
- `Claude reviewer`: review the task statement, the test contract, the diff, and the final result; block unclear or weak work.

Role-specific instructions live in `agents/gpt-lead.md` and `agents/claude-reviewer.md`.

## Stage-Gate Workflow

Use one local branch per task: `task/<short-name>`.

Complete work in this order:

1. Create `workitems/active/<task-name>/00-feature-packet.md`.
2. Create `workitems/active/<task-name>/01-test-contract.md`.
3. Implement the change and track deviations in `02-implementation-notes.md`.
4. Record review findings and disposition in `03-review-report.md`.

Do not merge to `main` until the full gate is complete: `spec -> tests -> implementation -> review`.

## Project Rules

- Keep all public headers under `include/cuda_test/`.
- Mirror subsystem layout between `include/` and `src/`.
- Preserve these module boundaries: `core`, `testing`, `benchmark`, `autotune`, `profiling`, `analysis`, `reporting`.
- Prioritize correctness before performance claims.
- Validate kernel output before accepting performance data.
- For benchmark and autotune experiments, use warm-up runs plus 30 measured runs and report a 95% confidence interval.
- Store accepted experimental evidence under `reports/`; keep reproducible scratch output out of version control unless explicitly accepted.

## Local Git Workflow

- Initialize and use git locally even without a remote.
- Keep `main` as the integration branch.
- Use commit prefixes that reflect the gate: `spec:`, `test:`, `impl:`, `review:`.
- Create annotated tags for milestones such as `mvp-scaffold`, `kernel-test-v1`, and `autotune-v1`.
- Use `git diff` as the default review surface for Claude.
- Record cross-cutting architecture or process decisions in `docs/decisions/` and reference the related commit hash in the note.

## Skills

Use project skills for repeatable workflows:

- `skills/feature-spec-contract/`
- `skills/cuda-test-harness/`
- `skills/cuda-benchmark-autotune/`
- `skills/nir-report-sync/`

Skills must not duplicate the global rules from this file. They should only narrow the workflow for a specific task type.

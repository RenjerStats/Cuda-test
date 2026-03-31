---
name: cuda-benchmark-autotune
description: Define benchmark, profiling, and autotune workflows for CUDA kernels in this repository. Use when planning or implementing Google Benchmark integration, launch-configuration search, staged timing breakdowns, candidate ranking, or performance evidence.
---

# Cuda Benchmark Autotune

## Overview

Plan reproducible performance work for kernels after correctness has already been locked down.

## Workflow

1. Read the benchmark-related sections in `plan/` and `docs/methodology/benchmark-protocol.md`.
2. Confirm that correctness is already covered by tests.
3. Define the candidate space: block sizes, grid logic, shared memory, and device assumptions.
4. Separate warm-up from measured runs.
5. Capture staged timings for H2D, kernel, D2H, and total runtime.
6. Rank candidates by the repository default policy unless the task packet overrides it.
7. Export evidence into `reports/` only after the result set is accepted.

## Default Expectations

- Use 30 measured runs by default.
- Report a 95% confidence interval.
- Compare tuned results against a declared baseline configuration.
- Explain why the winning configuration was selected.

## References

Read `references/benchmark-rules.md` for the default measurement and ranking policy.

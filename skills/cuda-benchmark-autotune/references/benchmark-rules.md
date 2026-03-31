# Benchmark And Autotune Rules

## Measurement Rules

- Run warm-up iterations before measured iterations.
- Use 30 measured runs by default.
- Report `mean`, `median`, `p95`, `ci95`, and `cv`.
- Split timings into H2D, kernel, D2H, and total when the task supports it.

## Candidate Rules

- State the baseline configuration explicitly.
- State the candidate generation policy explicitly.
- Exclude invalid candidates before measurement when device limits are known.

## Ranking Rules

- Rank by `median kernel_ms`.
- Break ties with `p95_ms`.
- Break remaining ties with `cv`.
- Record the reason for the selected winner in the result artifact.

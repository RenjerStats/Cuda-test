# Benchmark Protocol

Use this protocol for benchmark and autotune work until a more specific task document overrides it.

## Default Run Policy

- Perform warm-up runs before measuring.
- Measure 30 runs by default.
- Report a 95% confidence interval.
- Validate correctness before accepting timing data.

## Required Metrics

- `h2d_ms`
- `kernel_ms`
- `d2h_ms`
- `total_ms`
- `mean_ms`
- `median_ms`
- `p95_ms`
- `ci95_low`
- `ci95_high`
- `cv`

## Candidate Selection

- Rank candidates by median kernel time first.
- Break ties with `p95_ms`, then `cv`.
- Record the reason for the final choice in the report artifact.

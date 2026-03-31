# API Boundaries

## Public Surface

- Expose future public headers only from `include/cuda_test/`.
- Keep the stable public concepts aligned with the NIR plan: launch config, profiling breakdown, run stats, autotune spec, and result reporting.

## Internal Surface

- Keep CUDA runtime wrappers inside `core` unless a type must be user-facing.
- Keep Google Test and Google Benchmark adapters out of the core runtime layer.
- Keep report serialization separate from benchmark execution so experiment outputs remain testable.

## Change Control

- Record any public API change in the active feature packet before implementation.
- Revisit the test contract when an interface or metric definition changes.

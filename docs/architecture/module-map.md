# Module Map

The repository is organized around seven library subsystems.

## Modules

- `core`: low-level CUDA resource wrappers, device selection, and error handling.
- `testing`: Google Test integration, assertions, and kernel test helpers.
- `benchmark`: Google Benchmark integration and benchmark fixtures.
- `autotune`: candidate generation, ranking, and launch configuration search.
- `profiling`: staged timing breakdown for H2D, kernel, D2H, and total execution.
- `analysis`: derived metrics and bottleneck indicators.
- `reporting`: CSV/JSON serialization and experiment export.

## Layout Rules

- Public declarations belong under `include/cuda_test/<module>/`.
- Internal implementation belongs under `src/<module>/`.
- Tests live under `tests/`.
- Benchmarks live under `benchmarks/`.

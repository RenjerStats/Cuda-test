# Experiment Matrix

Track planned and completed experiments here or in derived report tables.

| Kernel | Scenario | Input Size | GPU | Baseline Config | Candidate Set | Status | Evidence Path |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `density_update` | representative autotune run | 4096 | `NVIDIA GeForce RTX 4070 Laptop GPU` | `block=128, grid=32` | `block={64,128,256}, wave={1,2}` | completed, single GPU | `reports/tables/single-gpu/density_update-autotune.csv` |
| `physics_integration` | representative autotune run | 4096 | `NVIDIA GeForce RTX 4070 Laptop GPU` | `block=128, grid=32` | `block={64,128,256}, wave={1,2}` | completed, single GPU | `reports/tables/single-gpu/physics_integration-autotune.csv` |
| `contact_flag` | representative autotune run | 4096 | `NVIDIA GeForce RTX 4070 Laptop GPU` | `block=128, grid=32` | `block={64,128,256}, wave={1,2}` | completed, single GPU | `reports/tables/single-gpu/contact_flag-autotune.csv` |
| `active_compaction` | representative autotune run | 4096 | `NVIDIA GeForce RTX 4070 Laptop GPU` | `block=128, grid=32` | `block={64,128,256}, wave={1,2}` | completed, single GPU | `reports/tables/single-gpu/active_compaction-autotune.csv` |
| `buffer_generation` | representative autotune run | 4096 | `NVIDIA GeForce RTX 4070 Laptop GPU` | `block=128, grid=32` | `block={64,128,256}, wave={1,2}` | completed, single GPU | `reports/tables/single-gpu/buffer_generation-autotune.csv` |
| `interval_intersection` | representative autotune run | 4096 | `NVIDIA GeForce RTX 4070 Laptop GPU` | `block=128, grid=32` | `block={64,128,256}, wave={1,2}` | completed, single GPU | `reports/tables/single-gpu/interval_intersection-autotune.csv` |

## Notes

- Cover at least 6 kernels from the target application.
- Phase 7 evidence is complete for one available NVIDIA GPU; the second-GPU comparison remains deferred until matching hardware is available.
- Keep the same input data between baseline and tuned comparisons.

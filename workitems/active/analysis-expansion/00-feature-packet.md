# Feature Packet

## Task

- Name: Post-MVP Phase 4 — Analysis Expansion
- Owner: GPT (lead), Claude (reviewer)
- Branch: `task/analysis-expansion`
- Status: spec-ready
- Vision link: `workitems/active/post-mvp-vision/00-vision.md` — решает проблему P4

## Research Link

- Related section(s) in `plan/Пояснительная записка.md`: §15 (профилирование и метрики узких мест), §7 (научная новизна — автоматическое снятие метрик)
- Related section(s) in vision: «P4 — Скудная аналитика», «Доступные метрики CUDA Runtime API»

## Objective

Расширить модуль `analysis` до набора метрик, которые одновременно:

1. реально помогают интерпретировать производительность CUDA-ядра;
2. доступны через CUDA Runtime API или как чистые производные вычисления поверх уже собранных данных;
3. могут быть позже использованы модулем `diagnostics-advisor`.

В этой фазе добавляются только те метрики, которые имеют практическую ценность без Nsight Compute: occupancy estimate, kernel function attributes (registers/local/shared memory limits), effective vs theoretical bandwidth, device memory headroom, block sensitivity, scaling exponent. Метрики уровня ALU utilization, cache hit rate, warp execution efficiency и instruction counts остаются за пределами scope, потому что Runtime API их не предоставляет.

`coalescing_sensitivity` из §15 записки сознательно не включается в эту фазу. Это не Runtime API query, а экспериментальная метрика, требующая отдельного strided-vs-linear benchmark сценария и, следовательно, относится скорее к benchmark methodology / diagnostics phase, чем к базовому расширению `analysis`.

## Scope

- In scope:
  - `include/cuda_test/analysis/occupancy.hpp`:
    - `OccupancyInfo` struct: `active_blocks_per_sm`, `active_warps_per_sm`, `max_warps_per_sm`, `occupancy_ratio`
    - `estimate_occupancy(const void* kernel_func, int block_size, std::size_t dynamic_smem, int device_id)` → `OccupancyInfo`
    - `suggest_block_size(kernel_func, std::size_t dynamic_smem, int block_size_limit, int device_id)` → `int`
      - template-wrapper над `cudaOccupancyMaxPotentialBlockSize`
      - reason: Runtime API даёт `cudaOccupancyMaxActiveBlocksPerMultiprocessor` через C-style symbol pointer, но `cudaOccupancyMaxPotentialBlockSize` в удобной форме экспонируется как C++ API
  - `include/cuda_test/analysis/kernel_attributes.hpp`:
    - `KernelAttributes` struct: `num_regs`, `shared_size_bytes`, `const_size_bytes`, `local_size_bytes`, `max_threads_per_block`, `max_dynamic_shared_size_bytes`, `ptx_version`, `binary_version`
    - `get_kernel_attributes(const void* kernel_func)` → `KernelAttributes`
  - `include/cuda_test/analysis/bandwidth.hpp`:
    - `BandwidthInfo` struct: `theoretical_gbps`, `achieved_gbps`, `utilization_ratio`
    - `estimate_bandwidth(std::size_t bytes_transferred, double kernel_ms, int device_id)` → `BandwidthInfo`
  - `include/cuda_test/analysis/memory_info.hpp`:
    - `MemoryPressure` struct: `free_bytes`, `total_bytes`, `usage_ratio`
    - `query_memory_pressure(int device_id)` → `MemoryPressure`
      - explicitly documented as runtime headroom estimate from `cudaMemGetInfo`, not exact per-kernel pressure attribution
  - Расширение `include/cuda_test/analysis/metrics.hpp`:
    - Существующий `transfer_compute_ratio()` — без изменений
    - Новый: `block_sensitivity(const std::vector<CandidateRecord>& candidates)` → `double` (max_median / min_median)
    - Новый: `scaling_exponent(const std::vector<std::pair<std::size_t, double>>& size_time_pairs)` → `double` (наклон log-log регрессии)
  - `include/cuda_test/analysis/fingerprint.hpp`:
    - `KernelFingerprint` struct, агрегирующий все метрики
    - `build_fingerprint(...)` — конструктор fingerprint из доступных данных
  - unit-тесты для вычислительных функций (bandwidth, scaling_exponent, block_sensitivity — host-only)
  - CUDA-тесты для occupancy, kernel_attributes, memory_info

- Out of scope:
  - Интерпретация fingerprint / система рекомендаций (задача diagnostics-advisor)
  - Интеграция с Pipeline (будет сделана в diagnostics-advisor через `.diagnose()`)
  - Сбор метрик через Nsight Compute API / CUPTI (за пределами НИР)
  - `coalescing_sensitivity` — требует отдельного benchmark сценария с контролируемым layout/access pattern, не просто Runtime API query

## Affected Areas

- Modules: `analysis`
- Public headers:
  - `include/cuda_test/analysis/occupancy.hpp` (новый)
  - `include/cuda_test/analysis/kernel_attributes.hpp` (новый)
  - `include/cuda_test/analysis/bandwidth.hpp` (новый)
  - `include/cuda_test/analysis/memory_info.hpp` (новый)
  - `include/cuda_test/analysis/fingerprint.hpp` (новый)
  - `include/cuda_test/analysis/metrics.hpp` (расширение)
- Umbrella header: добавить новые заголовки в `cuda_test.hpp`
- Dependencies: `core::DeviceInfo` (для theoretical bandwidth), `autotune::CandidateRecord` (для block_sensitivity)

## Interface Notes

### Новые типы

```cpp
namespace cuda_test::analysis {

struct OccupancyInfo {
    int active_blocks_per_sm = 0;
    int active_warps_per_sm = 0;
    int max_warps_per_sm = 0;
    double occupancy_ratio = 0.0;  // active_warps / max_warps, [0..1]
};

struct KernelAttributes {
    int num_regs = 0;
    std::size_t shared_size_bytes = 0;
    std::size_t const_size_bytes = 0;
    std::size_t local_size_bytes = 0;
    int max_threads_per_block = 0;
    int max_dynamic_shared_size_bytes = 0;
    int ptx_version = 0;
    int binary_version = 0;
};

struct BandwidthInfo {
    double theoretical_gbps = 0.0;
    double achieved_gbps = 0.0;
    double utilization_ratio = 0.0;  // achieved / theoretical, [0..1]
};

struct MemoryPressure {
    std::size_t free_bytes = 0;
    std::size_t total_bytes = 0;
    double usage_ratio = 0.0;  // (total - free) / total, [0..1]
};

struct KernelFingerprint {
    double transfer_compute_ratio = 0.0;
    double occupancy = 0.0;            // [0..1]
    double bandwidth_utilization = 0.0; // achieved/theoretical, clamped to [0..1]
    double cv = 0.0;
    double block_sensitivity = 0.0;
    double scaling_exponent = 0.0;     // 0 если не вычислялся
    int num_regs = 0;
    std::size_t local_size_bytes = 0;
    std::size_t shared_size_bytes = 0;
};

// Functions
OccupancyInfo estimate_occupancy(const void* kernel_func, int block_size,
                                  std::size_t dynamic_smem = 0,
                                  int device_id = 0);
template <typename KernelFunc>
int suggest_block_size(KernelFunc kernel_func,
                       std::size_t dynamic_smem = 0,
                       int block_size_limit = 0,
                       int device_id = 0);
KernelAttributes get_kernel_attributes(const void* kernel_func);
BandwidthInfo estimate_bandwidth(std::size_t bytes_transferred, double kernel_ms,
                                  int device_id = 0);
MemoryPressure query_memory_pressure(int device_id = 0);

// Derived metrics (host-only, pure computation)
double block_sensitivity(const std::vector<autotune::CandidateRecord>& candidates);
double scaling_exponent(const std::vector<std::pair<std::size_t, double>>& size_time_pairs);

} // namespace cuda_test::analysis
```

### Выбор метрик

- **Occupancy** полезен как индикатор способности скрывать latency, но не трактуется как прямой proxy performance.
- **Registers / local memory / static shared memory** полезны для диагностики register pressure, spills и launch limits.
- **Bandwidth utilization** полезен только при известной оценке `bytes_transferred`; без неё Runtime API сам по себе bandwidth не предоставляет.
- **Memory pressure** трактуется как headroom/capacity metric, а не как точная причина медленного kernel runtime.
- **Scaling exponent** и **block sensitivity** — производные метрики над уже собранными timing-данными, не требующие дополнительных CUDA API.

### Расчёт theoretical bandwidth

```
theoretical_gbps = (memory_clock_khz * 1000 * 2 * (bus_width_bits / 8)) / 1e9
```

Значения берутся из `cudaDeviceProp`: `memoryClockRate` (kHz) и `memoryBusWidth` (bits).

### Расчёт scaling_exponent

Линейная регрессия на `log(N)` vs `log(t)`:
```
exponent = Σ((log(Ni) - mean_logN)(log(ti) - mean_logt)) / Σ((log(Ni) - mean_logN)²)
```

Значения: `exponent ≈ 1.0` — линейное масштабирование (идеал), `> 1.2` — суперлинейное (contention).

### Расчёт block_sensitivity

```
block_sensitivity = max(median_kernel_ms) / min(median_kernel_ms)
```

По всем кандидатам автотюна. Значение `1.0` — нечувствителен, `> 1.3` — чувствителен.

## Risks

- **`cudaOccupancyMaxActiveBlocksPerMultiprocessor` требует `const void*` на kernel.** В type-erased KernelDescriptor указатель на ядро недоступен. Mitigation: occupancy API принимает raw function pointer напрямую; интеграция с Pipeline через опциональный `.kernel_func()` в descriptor (будущее расширение) или вызов отдельно.
- **`cudaOccupancyMaxPotentialBlockSize` неудобно приводить к `const void*`.** Mitigation: `suggest_block_size` оформляется как template-wrapper над Runtime C++ API, а не как `const void*`-функция.
- **Theoretical bandwidth может отличаться от реальной.** Memory clock rate из `cudaDeviceProp` — это базовая частота; boost может быть выше. Mitigation: документировать как оценку, не как точное значение; `utilization_ratio` трактовать как heuristic.
- **`cudaMemGetInfo` не даёт точную причину slowdown.** Документация NVIDIA указывает, что free-memory estimate зависит от текущего контекста/ОС и подвержен race conditions. Mitigation: использовать metric только как headroom indicator.
- **`scaling_exponent` требует прогона на нескольких размерах.** Данные нужно собрать заранее. Mitigation: функция принимает готовые пары (N, time); сбор данных — ответственность вызывающего кода или Pipeline.

## Acceptance Criteria

- [ ] `estimate_occupancy()` возвращает `OccupancyInfo` с `occupancy_ratio` в [0,1] и корректными warp-based полями для реального ядра
- [ ] `suggest_block_size()` возвращает кратное warp_size значение > 0
- [ ] `get_kernel_attributes()` возвращает `num_regs > 0` для реального ядра
- [ ] `estimate_bandwidth()` вычисляет `theoretical_gbps > 0` и `utilization_ratio` в [0,1]
- [ ] `query_memory_pressure()` возвращает `total_bytes > 0` и `usage_ratio` в [0,1]
- [ ] `block_sensitivity()` возвращает 1.0 для одинаковых кандидатов, >1.0 для разных
- [ ] `scaling_exponent()` возвращает ~1.0 для линейных данных (host-only тест с синтетическими парами)
- [ ] `scaling_exponent()` возвращает ~2.0 для квадратичных данных
- [ ] `KernelFingerprint` можно сконструировать со всеми полями
- [ ] `transfer_compute_ratio()` продолжает работать без изменений
- [ ] unit-тесты (host-only) покрывают bandwidth, block_sensitivity, scaling_exponent с синтетическими данными
- [ ] CUDA-тесты покрывают occupancy, kernel_attributes, memory_info с реальным ядром

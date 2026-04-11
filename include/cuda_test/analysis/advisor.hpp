#pragma once

#include "cuda_test/analysis/fingerprint.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace cuda_test::analysis {

struct Recommendation {
    std::string tag;
    std::string severity;
    std::string summary;
    std::string suggestion;
};

namespace detail {

inline void add_recommendation(std::vector<Recommendation>& recommendations,
                               std::string_view tag,
                               std::string_view severity,
                               std::string_view summary,
                               std::string_view suggestion) {
    recommendations.push_back(Recommendation{std::string(tag),
                                             std::string(severity),
                                             std::string(summary),
                                             std::string(suggestion)});
}

} // namespace detail

inline std::vector<Recommendation> diagnose(const KernelFingerprint& fingerprint) {
    std::vector<Recommendation> recommendations;

    if (fingerprint.transfer_compute_ratio > 3.0) {
        detail::add_recommendation(
            recommendations,
            "transfer_dominated",
            "critical",
            "Передачи между хостом и устройством доминируют во времени работы ядра.",
            "Сначала уменьшите трафик передач: держите промежуточные данные на устройстве, объединяйте мелкие "
            "копии, используйте закрепленную память хоста (pinned memory) и по возможности перекрывайте "
            "передачи вычислениями в отдельных потоках CUDA.");
    }

    if (fingerprint.has_occupancy && fingerprint.has_kernel_attributes && fingerprint.occupancy > 0.0 &&
        fingerprint.occupancy < 0.5 && fingerprint.num_regs > 32) {
        detail::add_recommendation(
            recommendations,
            "low_occupancy",
            "warning",
            "Низкая теоретическая заполняемость вместе с высоким расходом регистров указывает на слабое скрытие задержек.",
            "Проверьте расход регистров через --ptxas-options=-v, аккуратно попробуйте __launch_bounds__ или "
            "-maxrregcount и заново протестируйте размер блока в диапазоне 128-256 потоков.");
    }

    if (fingerprint.has_kernel_attributes && fingerprint.local_size_bytes > 0U) {
        detail::add_recommendation(
            recommendations,
            "local_memory_pressure",
            "warning",
            "Локальная память на поток ненулевая; такие обращения идут во внечиповую память.",
            "Посмотрите вывод ptxas по lmem и уменьшите крупные массивы на поток, динамическую индексацию или "
            "спиллы live-range, которые выталкивают данные из регистров.");
    }

    if (fingerprint.has_bandwidth_utilization && fingerprint.bandwidth_utilization > 0.8) {
        detail::add_recommendation(
            recommendations,
            "bandwidth_bound",
            "info",
            "Вероятно, ядро упирается в пропускную способность памяти устройства.",
            "Улучшите коалесцирование обращений, уберите лишний трафик глобальной памяти и используйте "
            "разделяемую память там, где она превращает обращения с шагом в последовательный или кэшируемый доступ. "
            "Проверьте конфликты банков в тайловом коде.");
    }

    if (fingerprint.cv > 0.15) {
        detail::add_recommendation(
            recommendations,
            "unstable_timing",
            "warning",
            "Разброс времени достаточно велик, чтобы ослабить выводы из бенчмарка.",
            "Уменьшите конкурирующую GPU-нагрузку и число контекстов, используйте более тихую среду "
            "бенчмаркинга или режим монопольного процесса (exclusive-process), и только потом увеличивайте число измерений.");
    }

    if (fingerprint.block_sensitivity > 1.3) {
        detail::add_recommendation(
            recommendations,
            "block_sensitive",
            "info",
            "Производительность заметно меняется при разных размерах блока.",
            "Оставьте размер блока настраиваемым, тестируйте значения кратные 32 и после каждого изменения "
            "ядра заново начинайте поиск в диапазоне 128-256 потоков.");
    }

    if (fingerprint.has_scaling_exponent && fingerprint.scaling_exponent > 1.2) {
        detail::add_recommendation(
            recommendations,
            "superlinear_scaling",
            "warning",
            "Время выполнения растет быстрее, чем ожидается для почти линейного масштабирования.",
            "Проверьте алгоритмическую сложность, последовательные участки, горячие точки по синхронизации или "
            "атомарным операциям и рост трафика памяти при увеличении размера задачи.");
    }

    // Keep these defensive non-negative guards so "well_utilized" does not fire
    // on incomplete or corrupted metrics that happen to satisfy the upper bounds.
    if (recommendations.empty() && fingerprint.transfer_compute_ratio >= 0.0 &&
        fingerprint.transfer_compute_ratio < 0.3 && fingerprint.has_occupancy &&
        fingerprint.occupancy >= 0.6 && fingerprint.cv >= 0.0 && fingerprint.cv < 0.1 &&
        fingerprint.block_sensitivity >= 0.0 &&
        fingerprint.block_sensitivity <= 1.2) {
        detail::add_recommendation(
            recommendations,
            "well_utilized",
            "info",
            "По доступным метрикам явных высокоуровневых узких мест не обнаружено.",
            "Дальнейший прирост, вероятно, потребует специфичной для ядра работы: параллелизма на уровне "
            "инструкций, изменений на уровне warp-алгоритма или более глубокого профилирования в Nsight Compute.");
    }

    return recommendations;
}

} // namespace cuda_test::analysis

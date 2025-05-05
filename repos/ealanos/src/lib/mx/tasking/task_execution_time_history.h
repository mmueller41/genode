#pragma once

#include <cstdint>

#ifdef USE_AVX2
#include <immintrin.h>
#else
#include <algorithm>
#include <array>
#endif

namespace mx::tasking {
#ifdef USE_AVX2
class TaskExecutionTimeHistory
{
public:
    TaskExecutionTimeHistory() noexcept { _history = _mm256_setzero_si256(); }

    ~TaskExecutionTimeHistory() noexcept = default;

    [[nodiscard]] constexpr std::uint8_t size() const noexcept { return 8U; }

    [[nodiscard]] std::uint8_t prefetch_distance(const std::uint32_t needed_cycles) noexcept
    {
        const auto needed_cycles_vector = _mm256_set1_epi32(std::int32_t(needed_cycles));

        /// Compare each item in the histry if the slot covers the needed cycles.
        const auto compared_cycles = _mm256_cmpgt_epi32(needed_cycles_vector, _history);

        /// Count the slots needed.
        const auto mask = _mm256_movemask_epi8(compared_cycles);
        const auto pop_count = _mm_popcnt_u32(mask);
        const auto prefetch_distance = pop_count >> 2U;

        return prefetch_distance;
    }

    void push(const std::uint32_t cycles) noexcept
    {
        /// Shift out the last task.
        const auto shifted = _mm256_alignr_epi8(_mm256_permute2x128_si256(_history, _history, 0x81), _history, 4);

        /// Add the task's cycles to the history.
        _history = _mm256_add_epi32(shifted, _mm256_set1_epi32(cycles));
    }

private:
    /// Last 8 task cycles (summed up = index(0) next task, index(1) = index(0) + next next task, etc.)
    alignas(64) __m256i _history;
};
#else
class TaskExecutionTimeHistory
{
public:
    TaskExecutionTimeHistory() noexcept = default;

    ~TaskExecutionTimeHistory() noexcept = default;

    [[nodiscard]] constexpr std::uint8_t size() const noexcept { return 8U; }

    [[nodiscard]] std::uint8_t prefetch_distance(const std::uint32_t needed_cycles) noexcept
    {
        auto cycles = _history[7U];
        for (auto i = 6; i > 0; --i)
        {
            if (cycles >= needed_cycles)
            {
                return 8 - (i + 1);
            }

            cycles += _history[i];
        }

        return 8;
    }

    void push(const std::uint32_t cycles) noexcept
    {

        /// Shift out the last task.
        std::rotate(_history.begin(), _history.begin() + 1, _history.end());

        /// Add the task's cycles to the history.
        _history[7U] = cycles;
    }

private:
    /// Last 8 task cycles (summed up = index(0) next task, index(1) = index(0) + next next task, etc.)
    alignas(64) std::array<std::uint32_t, 8U> _history{0U};
};
#endif
} // namespace mx::tasking
#pragma once

#include <array>
#include <atomic>
#include <mx/resource/annotation.h>
#include <mx/resource/ptr.h>

namespace mx::tasking {
/**
 * Stores usage predictions.
 */
class TaskPoolOccupancy
{
public:
    constexpr TaskPoolOccupancy() = default;
    ~TaskPoolOccupancy() = default;

    /**
     * Adds the given predicted usage.
     * @param predicted_usage Predicted usage.
     */
    void predict(const mx::resource::expected_access_frequency predicted_usage) noexcept
    {
        _predicted_usage_counter[static_cast<std::uint8_t>(predicted_usage)].fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Subtracts the given predicted usage.
     * @param predicted_usage  Predicted usage.
     */
    void revoke(const mx::resource::expected_access_frequency predicted_usage) noexcept
    {
        _predicted_usage_counter[static_cast<std::uint8_t>(predicted_usage)].fetch_sub(1, std::memory_order_relaxed);
    }

    /**
     * @return True, when at least one prediction was "excessive".
     */
    [[nodiscard]] bool has_excessive_usage_prediction() const noexcept
    {
        return has_at_least_one<mx::resource::expected_access_frequency::excessive>();
    }

    /**
     * @return The highest predicted usage.
     */
    explicit operator mx::resource::expected_access_frequency() const noexcept
    {
        if (has_at_least_one<mx::resource::expected_access_frequency::excessive>())
        {
            return mx::resource::expected_access_frequency::excessive;
        }

        if (has_at_least_one<mx::resource::expected_access_frequency::high>())
        {
            return mx::resource::expected_access_frequency::high;
        }

        if (has_at_least_one<mx::resource::expected_access_frequency::normal>())
        {
            return mx::resource::expected_access_frequency::normal;
        }

        return mx::resource::expected_access_frequency::unused;
    }

private:
    // Counter of predicted usages.
    std::array<std::atomic_uint64_t, 4U> _predicted_usage_counter{0U};

    /**
     * @return True, when at least one usage as given by the template was predicted.
     */
    template <mx::resource::expected_access_frequency U> [[nodiscard]] bool has_at_least_one() const noexcept
    {
        return _predicted_usage_counter[static_cast<std::uint8_t>(U)].load(std::memory_order_relaxed) > 0;
    }
};
} // namespace mx::tasking
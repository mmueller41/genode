#pragma once
#include "config.h"
#include <bitset>
#include <cstdint>

namespace mx::tasking {
/**
 * Persists the channel load for the last 64 requests.
 */
class Load
{
public:
    constexpr Load() = default;
    ~Load() = default;

    void set(const std::uint16_t count_withdrawed_task) noexcept
    {
        _load = count_withdrawed_task / float(config::task_buffer_size());
    }

    [[nodiscard]] float get() const noexcept { return _load; }

private:
    float _load{0U};
};
} // namespace mx::tasking
#pragma once
#include <cstdint>
#include <limits>

namespace mx::tasking {
class PrefetchDistance
{
public:
    [[nodiscard]] constexpr static PrefetchDistance make_automatic() noexcept
    {
        return PrefetchDistance{std::numeric_limits<std::uint8_t>::max()};
    }

    constexpr explicit PrefetchDistance(const std::uint8_t prefetch_distance) noexcept
        : _prefetch_distance(prefetch_distance)
    {
    }

    constexpr PrefetchDistance(const PrefetchDistance &) noexcept = default;

    ~PrefetchDistance() noexcept = default;

    PrefetchDistance &operator=(PrefetchDistance &&) noexcept = default;

    [[nodiscard]] bool is_enabled() const noexcept { return _prefetch_distance > 0U; }

    [[nodiscard]] bool is_automatic() const noexcept
    {
        return _prefetch_distance == std::numeric_limits<std::uint8_t>::max();
    }

    [[nodiscard]] bool is_fixed() const noexcept { return is_enabled() && is_automatic() == false; }

    [[nodiscard]] std::uint8_t fixed_distance() const noexcept { return _prefetch_distance; }

private:
    std::uint8_t _prefetch_distance;
};
} // namespace mx::tasking
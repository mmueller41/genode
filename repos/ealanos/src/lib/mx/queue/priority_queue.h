#pragma once

#include <array>
#include <cassert>
#include <mx/tasking/priority.h>

namespace mx::queue {
template <class Q, tasking::priority MIN_PRIORITY, tasking::priority MAX_PRIORITY> class PriorityQueue
{
public:
    PriorityQueue() = default;
    ~PriorityQueue() = default;

    template <tasking::priority P> Q &get() noexcept
    {
        static_assert(static_cast<std::uint8_t>(P) >= static_cast<std::uint8_t>(MIN_PRIORITY));
        static_assert(static_cast<std::uint8_t>(P) <= static_cast<std::uint8_t>(MAX_PRIORITY));

        return _queues[static_cast<std::uint8_t>(P) - static_cast<std::uint8_t>(MIN_PRIORITY)];
    }

    template <tasking::priority P> const Q &get() const noexcept
    {
        static_assert(static_cast<std::uint8_t>(P) >= static_cast<std::uint8_t>(MIN_PRIORITY));
        static_assert(static_cast<std::uint8_t>(P) <= static_cast<std::uint8_t>(MAX_PRIORITY));

        return _queues[static_cast<std::uint8_t>(P) - static_cast<std::uint8_t>(MIN_PRIORITY)];
    }

    Q &get(const tasking::priority priority) noexcept
    {
        assert(priority >= static_cast<std::uint8_t>(MIN_PRIORITY));
        assert(priority <= static_cast<std::uint8_t>(MAX_PRIORITY));

        return _queues[static_cast<std::uint8_t>(priority) - static_cast<std::uint8_t>(MIN_PRIORITY)];
    }

    const Q &get(const tasking::priority priority) const noexcept
    {
        assert(priority >= static_cast<std::uint8_t>(MIN_PRIORITY));
        assert(priority <= static_cast<std::uint8_t>(MAX_PRIORITY));

        return _queues[static_cast<std::uint8_t>(priority) - static_cast<std::uint8_t>(MIN_PRIORITY)];
    }

private:
    std::array<Q, static_cast<std::uint8_t>(MAX_PRIORITY) - static_cast<std::uint8_t>(MIN_PRIORITY) + 1U> _queues{};
};
} // namespace mx::queue
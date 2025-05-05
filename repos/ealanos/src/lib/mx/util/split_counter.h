#pragma once

#include "aligned_t.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace mx::util {
template <typename T, std::uint16_t MAX, std::uint16_t GROUPS> class split_counter
{
public:
    constexpr split_counter() noexcept = default;
    ~split_counter() noexcept = default;

    void add(const std::uint16_t id, const T value) { _groups[id / GROUPS].value().fetch_add(value); }

    void sub(const std::uint16_t id, const T value) { _groups[id / GROUPS].value().fetch_sub(value); }

    T load() const noexcept
    {
        auto value = T{0};

        for (const auto &val : _groups)
        {
            value += val.value().load();
        }

        return value;
    }

    T sub_and_load(const std::uint16_t id, const T value)
    {
        sub(id, value);
        return load();
    }

    T add_and_load(const std::uint16_t id, const T value)
    {
        add(id, value);
        return load();
    }

private:
    std::array<aligned_t<std::atomic<T>>, MAX / GROUPS> _groups;
};
} // namespace mx::util
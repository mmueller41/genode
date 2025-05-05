#pragma once

#include <cstdint>

namespace mx::tasking {
enum priority : std::uint8_t
{
    low = 0U,
    normal = 1U
};
}
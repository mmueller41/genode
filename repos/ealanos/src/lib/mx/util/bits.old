#pragma once
#include <cstdint>

namespace mx::util {
inline long int bit_scan_forward(std::uint64_t val)
{
    if (!val)
        return -1;

    asm volatile("bsf %1, %0" : "=r"(val) : "rm"(val));

    return val;
}
}
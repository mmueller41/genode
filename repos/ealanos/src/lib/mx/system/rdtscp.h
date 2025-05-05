#pragma once

#include <cstdint>

namespace mx::system {
class RDTSCP
{
public:
    [[nodiscard]] inline static std::uint64_t begin() noexcept
    {
        std::uint32_t high, low;

        asm volatile("CPUID\n\t"
                     "RDTSC\n\t"
                     "mov %%edx, %0\n\t"
                     "mov %%eax, %1\n\t"
                     : "=r"(high), "=r"(low)::"%rax", "%rbx", "%rcx", "%rdx");

        return (std::uint64_t(high) << 32) | low;
    }

    [[nodiscard]] inline static std::uint64_t end() noexcept
    {
        std::uint32_t high, low;

        asm volatile("RDTSCP\n\t"
                     "mov %%edx, %0\n\t"
                     "mov %%eax, %1\n\t"
                     "CPUID\n\t"
                     : "=r"(high), "=r"(low)::"%rax", "%rbx", "%rcx", "%rdx");

        return (std::uint64_t(high) << 32) | low;
    }
};
} // namespace mx::system
#pragma once
#include <cstdint>
#include <iostream>

namespace mx::system {
/**
 * Encapsulates compiler builtins.
 */
class builtin
{
public:
    /**
     * Generates a pause/yield cpu instruction, independently
     * of the hardware.
     */
    static void pause() noexcept
    {
#if defined(__x86_64__) || defined(__amd64__)
        __builtin_ia32_pause();
#elif defined(__arm__)
        asm("YIELD");
#endif
    }

    [[nodiscard]] static std::uint32_t clz(const std::uint32_t number) noexcept { return __builtin_clz(number); }
    [[nodiscard]] static std::uint64_t clz(const std::uint64_t number) noexcept { return __builtin_clzll(number); }
};
} // namespace mx::system
#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <unistd.h>

namespace mx::system {
/**
 * Encapsulates cache operations like prefetching.
 *
 * Further documentation on Intel: https://www.felixcloutier.com/x86/prefetchh
 */
class cache
{
public:
    [[nodiscard]] static constexpr auto line_size() noexcept { return 64U; }

    enum level : std::uint8_t
    {
        L1 = 3U,
        ALL = 3U,
        L2 = 2U,
        L3 = 1U,
        NTA = 0U
    };
    enum access : std::uint8_t
    {
        read = 0U,
        write = 1U
    };

    /**
     * Prefetches a single cache line into a given prefetch level.
     *
     * @tparam L Wanted cache level.
     * @tparam A Access to the cache line whether read or write.
     * @param address Address of the memory which should be prefetched.
     */
    template <level L, access A = access::read, std::uint8_t C>
    static void prefetch(const std::int64_t *address) noexcept
    {
        constexpr auto items_per_cacheline = line_size() / sizeof(std::int64_t);
        for (auto i = 0U; i < C * items_per_cacheline; i += items_per_cacheline)
        {
            __builtin_prefetch(&address[i], static_cast<std::uint8_t>(A), static_cast<std::uint8_t>(L));
        }
    }

    template <level L> [[nodiscard]] static std::uint64_t size()
	{
		/* TODO: Get cache sizes, maybe with CPUID? */
        if constexpr (L == level::L1)
        {
            if (_cache_size_cache[0U] == 0U)
            {
                //_cache_size_cache[0U] = sysconf(_SC_LEVEL1_DCACHE_SIZE);
            }
            return _cache_size_cache[0U];
        }
        else if constexpr (L == level::L2)
        {
            if (_cache_size_cache[1U] == 0U)
            {
                //_cache_size_cache[1U] = sysconf(_SC_LEVEL2_CACHE_SIZE);
            }
            return _cache_size_cache[1U];
        }
        else if constexpr (L == level::L3)
        {
            if (_cache_size_cache[2U] == 0U)
            {
                //_cache_size_cache[2U] = sysconf(_SC_LEVEL3_CACHE_SIZE);
            }
            return _cache_size_cache[2U];
        }
        else
        {
            return 0U;
        }
    }

    template <level L, access A = access::read, std::uint32_t S>
    static void prefetch_range(const std::int64_t *address) noexcept
    {
        if constexpr (S <= 64U)
        {
            prefetch<L, A, 1U>(address);
        }
        else if constexpr (S == 128U)
        {
            prefetch<L, A, 2U>(address);
        }
        else if constexpr (S == 192U)
        {
            prefetch<L, A, 3U>(address);
        }
        else if constexpr (S == 256U)
        {
            prefetch<L, A, 4U>(address);
        }
        else if constexpr (S == 256U)
        {
            prefetch<L, A, 4U>(address);
        }
        else if constexpr (S == 320U)
        {
            prefetch<L, A, 5U>(address);
        }
        else if constexpr (S == 384U)
        {
            prefetch<L, A, 6U>(address);
        }
        else if constexpr (S == 448U)
        {
            prefetch<L, A, 7U>(address);
        }
        else if constexpr (S == 512U)
        {
            prefetch<L, A, 8U>(address);
        }
        else if constexpr (S == 1024U)
        {
            prefetch<L, A, 16U>(address);
        }
        else
        {
            prefetch_range<L, A>(address, S);
        }
    }

    /**
     * Prefetches a range of cache lines into the given cache level.
     *
     * @tparam L Wanted cache level.
     * @tparam A Access to the cache line whether read or write.
     * @param address Address of the memory which should be prefetched.
     * @param size Size of the accessed memory.
     */
    template <level L, access A = access::read>
    static void prefetch_range(const std::int64_t *address, const std::uint32_t size) noexcept
    {
        const auto cache_lines_to_prefetch = size / cache::line_size();
        switch (cache_lines_to_prefetch)
        {
        case 4:
            prefetch<L, A, 4U>(address);
            break;
        case 2:
            prefetch<L, A, 2U>(address);
            break;
        case 1:
            prefetch<L, A, 1U>(address);
            break;
        case 8:
            prefetch<L, A, 8U>(address);
            break;
        case 3:
            prefetch<L, A, 3U>(address);
            break;
        case 12:
            prefetch<L, A, 12U>(address);
            break;
        case 16:
            prefetch<L, A, 16U>(address);
            break;
        case 5:
            prefetch<L, A, 5U>(address);
            break;
        case 6:
            prefetch<L, A, 6U>(address);
            break;
        case 7:
            prefetch<L, A, 7U>(address);
            break;
        case 9:
            prefetch<L, A, 9U>(address);
            break;
        case 10:
            prefetch<L, A, 10U>(address);
            break;
        case 11:
            prefetch<L, A, 11U>(address);
            break;
        case 13:
            prefetch<L, A, 13U>(address);
            break;
        case 14:
            prefetch<L, A, 14U>(address);
            break;
        case 15:
            prefetch<L, A, 15U>(address);
            break;
        default:
            break;
        }
    }

private:
    static inline std::array<std::uint64_t, 3U> _cache_size_cache{{0U, 0U, 0U}};
};
} // namespace mx::system
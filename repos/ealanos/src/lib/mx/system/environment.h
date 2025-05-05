#pragma once

#include <fstream>

namespace mx::system {
/**
 * Encapsulates functionality of the (Linux) system.
 */
class Environment
{
public:
    /**
     * @return True, if NUMA balancing is enabled by the system.
     */
    static bool is_numa_balancing_enabled()
    {
        return false; /* EalánOS has no NUMA balancing, yet.*/
    }

    static constexpr auto is_sse2()
    {
#ifdef USE_SSE2
        return true;
#else
        return false;
#endif // USE_SSE2
    }

    static constexpr auto is_debug()
    {
#ifdef NDEBUG
        return false;
#else
        return true;
#endif // NDEBUG
    }
};
} // namespace mx::system
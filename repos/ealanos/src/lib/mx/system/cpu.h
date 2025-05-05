#pragma once

#include <algorithm>
#include <cstdint>
#include <mx/tasking/config.h>
#include <unordered_map>
#include <tukija/syscall-generic.h>

namespace mx::system {
/**
 * Encapsulates methods for retrieving information
 * about the hardware landscape.
 */
class cpu
{
public:
    /**
     * @return Core where the caller is running.
     */
    [[nodiscard]] static std::uint16_t core_id() { return Tukija::Cip::cip()->get_cpu_index(); }

    /**
     * Reads the NUMA region identifier of the given core.
     *
     * @param core_id Id of the core.
     * @return Id of the NUMA region the core stays in.
     */
    [[nodiscard]] static std::uint8_t node_id(const std::uint16_t core_id)
    {
        return Tukija::Tip::tip()->domain_of_idx(core_id);
    }

    /**
     * Reads the NUMA region identifier of the current core.
     *
     * @return Id of the NUMA region the core stays in.
     */
    [[nodiscard]] static std::uint8_t node_id() { return Tukija::Tip::tip()->domain_of_loc(Genode::Thread::myself()->affinity()); }

    /**
     * @return The greatest NUMA region identifier.
     */
    [[nodiscard]] static std::uint8_t max_node_id() { return Tukija::Tip::tip()->num_domains(); }

    /**
     * @return Number of available cores.
     */
    [[nodiscard]] static std::uint16_t count_cores() { return std::uint16_t(Tukija::Cip::cip()->habitat_affinity.total()); }
private:
};
} // namespace mx::system
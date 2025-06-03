#pragma once

#include <tukija/syscall-generic.h>
#include <base/affinity.h>
#include <base/thread.h>

#include <algorithm>
#include <cstdint>
#include "environment.h"

#include <mx/memory/config.h>

namespace mx::system {
/**
 * Encapsulates methods for retrieving information
 * about the hardware landscape.
 */
class topology
{
public:
    /**
     * @return Core where the caller is running.
     */
    static std::uint16_t core_id() 
    {
        return std::uint16_t(Genode::Thread::myself()->affinity().xpos());
    }

    /**
     * Reads the NUMA region identifier of the given core.
     *
     * @param core_id Id of the core.
     * @return Id of the NUMA region the core stays in.
     */
    static std::uint8_t node_id(const std::uint16_t core_id) { return std::uint8_t(Tukija::Tip::tip()->domain_of_idx(core_id)); }

    /**
     * @return The greatest NUMA region identifier.
     */
    static std::uint8_t max_node_id() { return std::uint8_t(Tukija::Tip::tip()->num_domains()) > mx::memory::config::max_numa_nodes() ? mx::memory::config::max_numa_nodes() - 1 : std::uint8_t(Tukija::Tip::tip()->num_domains() - 1); }

    /**
     * @return Number of available cores.
     */
	static std::uint16_t count_cores()
	{
		return std::uint16_t(Tukija::Cip::cip()->habitat_affinity.total());
	}
};
} // namespace mx::system
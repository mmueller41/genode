#pragma once
#include <chrono>
#include <iterator>

namespace mx::memory {
class config
{
public:
    /**
     * @return Number of maximal provided NUMA regions.
     */
    static constexpr std::uint8_t max_numa_nodes() { return 8U; }

    /**
     * @return Cycles for prefetching a single cache line.
     */
    static constexpr std::uint32_t latency_per_prefetched_cache_line() { return 290U; }

    /**
     * @return Interval of each epoch, if memory reclamation is used.
     */
    static constexpr std::chrono::milliseconds epoch_interval() { return std::chrono::milliseconds(50U); }

    /**
     * @return True, if garbage is removed local.
     */
	static constexpr bool local_garbage_collection() { return false; }

	static constexpr std::size_t min_block_size() { return 64; }
	static constexpr std::size_t superblock_cutoff() { return 1024 * min_block_size();}
};
} // namespace mx::memory

#pragma once
#include "alignment_helper.h"
#include <cstdint>
#include <cstdlib>
#include <array>
#include <cstring>

#include <ealanos/memory/hamstraaja.h>

#include <mx/system/topology.h>
#include <mx/system/environment.h>
#include <mx/memory/config.h>

#include <base/log.h>

namespace mx::memory {
    using Alloc = Ealan::Memory::Hamstraaja<mx::memory::config::block_size(), mx::memory::config::hyperblock_cutoff()>;
/**
 * The global heap represents the heap, provided by the OS.
 */
class GlobalHeap
{

public:

    alignas(64) static Ealan::Memory::Hamstraaja<config::block_size(), config::hyperblock_cutoff()> *_alloc;


    /**
     * Allocates the given size on the given NUMA node.
     *
     * @param numa_node_id ID of the NUMA node, the memory should allocated on.
     * @param size  Size of the memory to be allocated.
     * @return Pointer to allocated memory.
     */
    static void *allocate(const std::uint8_t numa_node_id, const std::size_t size)
    {
        return _alloc->alloc(size, numa_node_id);
	}

    /**
     * Allocates the given memory aligned to the cache line
     * with a multiple of the alignment as a size.
     * The allocated memory is not NUMA aware.
     * @param size Size to be allocated.
     * @return Allocated memory
     */
    static void *allocate_cache_line_aligned(const std::size_t size)
    {
        return _alloc->alloc(size);
    }


    /**
     * Frees the given memory.
     *
     * @param memory Pointer to memory.
     * @param size Size of the allocated object.
     */
    static void free(void *memory)  { _alloc->free(memory); }
};
} // namespace mx::memory

alignas(64) Ealan::Memory::Hamstraaja<mx::memory::config::block_size(), mx::memory::config::hyperblock_cutoff()> *mx::memory::GlobalHeap::_alloc;

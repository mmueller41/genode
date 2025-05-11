#pragma once

#include "mx/memory/global_heap.h"
#include <cstdint>
#include <cstdlib>
#include <mx/system/cache.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mx::memory {
/**
 * Interface for task allocators (e.g. using systems malloc
 * or the internal allocator).
 */
class TaskAllocatorInterface
{
public:
    constexpr TaskAllocatorInterface() noexcept = default;
    virtual ~TaskAllocatorInterface() noexcept = default;

    /**
     * Allocates memory for the given core.
     * @param worker_id Worker to allocate memory for.
     * @return Allocated memory.
     */
    [[nodiscard]] virtual void *allocate(std::uint16_t worker_id) = 0;

    /**
     * Frees the memory at the given core.
     * @param worker_id Worker to store free memory.
     * @param address Address to free.
     */
    virtual void free(std::uint16_t worker_id, void *address) noexcept = 0;

    [[nodiscard]] virtual std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>
    allocated_chunks() = 0;
};

/**
 * Task allocator using the systems (aligned_)malloc/free interface.
 */
template <std::size_t S> class SystemTaskAllocator final : public TaskAllocatorInterface
{
public:
    constexpr SystemTaskAllocator() noexcept = default;
    ~SystemTaskAllocator() noexcept override = default;

    /**
     * @return Allocated memory using systems malloc (but aligned).
     */
    [[nodiscard]] void *allocate(const std::uint16_t /*worker_id*/) override
    {
        return memory::GlobalHeap::allocate_cache_line_aligned(S);
    }

    /**
     * Frees the given memory using systems free.
     * @param address Memory to free.
     */
    void free(const std::uint16_t /*worker_id*/, void *address) noexcept override { memory::GlobalHeap::free(address, 0, 0); }

    [[nodiscard]] std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>
    allocated_chunks() override
    {
        return std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>{};
    }
};
} // namespace mx::memory
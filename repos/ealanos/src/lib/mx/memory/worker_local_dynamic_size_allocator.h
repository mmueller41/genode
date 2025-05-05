#pragma once

#include "config.h"
#include "global_heap.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <mx/queue/mpsc.h>
#include <mx/tasking/config.h>
#include <mx/tasking/task.h>
#include <mx/util/core_set.h>
#include <set>

namespace mx::memory::dynamic::local {
/**
 * Header of a free element.
 */
class FreeHeader
{
public:
    constexpr FreeHeader(const std::size_t size, const std::uint8_t numa_node_id, const std::uint32_t block_id) noexcept
        : _size(size), _numa_node_id(numa_node_id), _block_id(block_id)
    {
    }
    ~FreeHeader() noexcept = default;

    [[nodiscard]] std::size_t size() const noexcept { return _size; }
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return _numa_node_id; }
    [[nodiscard]] std::uint32_t block_id() const noexcept { return _block_id; }
    [[nodiscard]] FreeHeader *next() const noexcept { return _next; }

    void next(FreeHeader *next) noexcept
    {
        assert(next != this);
        _next = next;
    }

    [[nodiscard]] bool is_right_neighbour(FreeHeader *possible_right_neighbour)
    {
        assert(this->_block_id == possible_right_neighbour->_block_id);

        return (std::uintptr_t(this) + _size) == std::uintptr_t(possible_right_neighbour);
    }

    void append(FreeHeader *other) { grow(other->_size); }

    void grow(const std::size_t size) { _size += size; }

private:
    /// Size of the full block, including the header size.
    /// We include the header size because that is the size that is usable for allocation.
    std::size_t _size;

    /// Id of the numa node the block was allocated in. Needed when returning
    /// remote free memory.
    const std::uint8_t _numa_node_id;

    /// Id of the block within the worker heap.
    const std::uint32_t _block_id;

    /// Will be used for allocation within allocation block
    /// and for queue in remote free list.
    FreeHeader *_next{nullptr};
};

/**
 * Header of a block that is allocated.
 */
class AllocationHeader
{
public:
    constexpr AllocationHeader(const std::size_t size, const std::uint16_t unused_size_before_header,
                               const std::uint16_t worker_id, const std::uint8_t numa_node_id,
                               const std::uint32_t block_id) noexcept
        : _size(size), _unused_size_before_header(unused_size_before_header), _worker_id(worker_id),
          _numa_node_id(numa_node_id), _block_id(block_id)
    {
    }

    ~AllocationHeader() noexcept = default;

    [[nodiscard]] std::size_t size() const noexcept { return _size; }
    [[nodiscard]] std::uint16_t unused_size_before_header() const noexcept { return _unused_size_before_header; }
    [[nodiscard]] std::uint16_t worker_id() const noexcept { return _worker_id; }
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return _numa_node_id; }
    [[nodiscard]] std::uint32_t block_id() const noexcept { return _block_id; }

    [[nodiscard]] FreeHeader *to_free_header() const noexcept
    {
        const auto size = _size + _unused_size_before_header + sizeof(AllocationHeader);
        return new (reinterpret_cast<void *>(std::uintptr_t(this) - _unused_size_before_header))
            FreeHeader(size, _numa_node_id, _block_id);
    }

private:
    /// Size of the block after the header.
    const std::size_t _size;

    /// Size in front of the header that is not used but needed for alignment.
    const std::uint16_t _unused_size_before_header;

    /// Id of the worker.
    const std::uint16_t _worker_id;

    /// Numa region the block was allocated in.
    std::uint8_t _numa_node_id;

    /// Id of the block within the worker heap.
    const std::uint32_t _block_id;
};

/**
 * A block allocated from global memory into a worker-local heap.
 */
class AllocatedBlock
{
private:
    class FreeHeaderDescriptor
    {
    public:
        constexpr FreeHeaderDescriptor(FreeHeader *header, const std::size_t size) noexcept
            : _header(header), _size(size)
        {
        }

        ~FreeHeaderDescriptor() noexcept = default;

        [[nodiscard]] FreeHeader *header() const noexcept { return _header; }
        [[nodiscard]] std::size_t size() const noexcept { return _size; }

        void grow(const std::size_t size) noexcept
        {
            _size += size;
            _header->grow(size);
        }

        bool operator<(const FreeHeaderDescriptor other) const noexcept
        {
            return std::uintptr_t(_header) < std::uintptr_t(other._header);
        }

    private:
        FreeHeader *_header;
        std::size_t _size;
    };

public:
    static inline constexpr auto DEFAULT_SIZE_IN_BYTES = 1024UL * 1024UL * 128U;

    AllocatedBlock(const std::uint32_t id, const std::uint8_t numa_node_id, const std::size_t size, void *data) noexcept
        : _id(id), _numa_node_id(numa_node_id), _size(size), _data(data)
    {
        _free_header.insert(FreeHeaderDescriptor{new (_data) FreeHeader(size, 0U, id), size});
    }

    AllocatedBlock(AllocatedBlock &&other) noexcept
        : _id(other._id), _numa_node_id(other._numa_node_id), _size(other._size), _data(std::exchange(other._data, nullptr)),
          _free_header(std::move(other._free_header))
    {
    }

    ~AllocatedBlock()
    {
        if (_data != nullptr)
        {
            GlobalHeap::free(std::exchange(_data, nullptr), _size, _numa_node_id);
        }
    }

    AllocatedBlock &operator=(AllocatedBlock &&other) noexcept
    {
        _id = other._id;
        _size = other._size;
        _data = std::exchange(other._data, nullptr);
        _free_header = std::move(other._free_header);
        return *this;
    }

    [[nodiscard]] std::uint32_t id() const noexcept { return _id; }
    [[nodiscard]] void *data() const noexcept { return _data; }
    [[nodiscard]] std::size_t size() const noexcept { return _size; }

    [[nodiscard]] void *allocate(std::uint16_t worker_id, std::uint8_t numa_node_id, std::size_t alignment,
                                 std::size_t size);
    void free(AllocationHeader *allocation_header);
    void refund(FreeHeader *free_header);

    [[nodiscard]] bool is_free() const noexcept
    {
        return _free_header.size() == 1U && _free_header.begin()->size() == _size;
    }

    [[nodiscard]] std::tuple<std::set<FreeHeaderDescriptor>::iterator, bool, std::size_t, std::size_t> find_free_header(
        std::size_t alignment, std::size_t size) const;

private:
    std::uint32_t _id;
    std::uint8_t _numa_node_id;
    std::size_t _size;
    void *_data;
    std::set<FreeHeaderDescriptor> _free_header;
};

class alignas(64) WorkerHeap
{
public:
    WorkerHeap(std::uint16_t worker_id, std::uint8_t numa_node_id);
    WorkerHeap(WorkerHeap &&other) noexcept;
    ~WorkerHeap() = default;

    /**
     * Allocates memory from the list of allocated blocks,
     * or, when no memory available, from the remote free list.
     * If, however, no memory is available, a new block is allocated.
     *
     * @param numa_node_id NUMA node id to allocate memory for.
     * @param alignment Alignment of the allocated address.
     * @param size Size to allocate.
     * @return Pointer to the allocated memory.
     */
    void *allocate(std::uint8_t numa_node_id, std::size_t alignment, std::size_t size);

    /**
     * Frees memory from a remote worker.
     * The memory will be freed lazy, using
     * a list that will be used for allocation.
     *
     * @param calling_numa_id NUMA node id of the freeing worker.
     * @param allocated_item Header to the allocation.
     */
    void free(const std::uint8_t calling_numa_id, AllocationHeader *allocated_item)
    {
        auto *free_header = allocated_item->to_free_header();
        _remote_free_lists[calling_numa_id].push_back(free_header);
    }

    /**
     * Frees a local-worker allocated block.
     *
     * @param allocated_block Header to the allocation.
     */
    void free(AllocationHeader *allocated_block);

    /**
     * Releases all free blocks.
     */
    void release_free_memory();

    /**
     * Releases all blocks.
     */
    void release_all_memory();

    /**
     * Refunds memory from the remote list.
     */
    void refund_remote_freed_memory();

    void initialize(std::uint8_t numa_nodes);

    [[nodiscard]] bool is_free() const noexcept;

private:
    const std::uint16_t _worker_id;
    const std::uint8_t _numa_node_id;
    std::uint32_t _next_block_id{0U};

    /// Every worker can allocate blocks for every numa region.
    std::array<std::vector<AllocatedBlock>, config::max_numa_nodes()> _allocated_blocks;

    /// Index for every numa node that points from block_id to index in _allocated_blocks.
    std::array<std::unordered_map<std::uint32_t, std::uint64_t>, config::max_numa_nodes()> _allocated_block_indices;

    std::array<queue::MPSC<FreeHeader>, config::max_numa_nodes()> _remote_free_lists;
};

class Allocator
{
public:
    Allocator(const util::core_set &cores);
    ~Allocator();

    void initialize_heap(std::uint16_t worker_id, std::uint8_t count_numa_nodes);

    void *allocate(std::uint16_t worker_id, std::uint8_t numa_node_id, std::size_t alignment, std::size_t size);
    void free(std::uint16_t calling_worker_id, void *pointer);

    /**
     * Frees the memory always as a "remote" caller.
     * For performance reason, this should be used carefully!
     *
     * @param pointer Pointer to the data.
     */
    void free(void *pointer);

    /**
     * Resets all worker-local allocators.
     *
     * @param cores New core set (may change in contrast to the current).
     * @param force_free_memory If set to true, memory should be freed.
     */
    void reset(const util::core_set &cores, bool force_free_memory);

    /**
     * Cleans the remote free'd memory of a worker-local heap.
     *
     * @param worker_id Worker id to clean up.
     */
    void clean_up_remote_freed_memory(const std::uint16_t worker_id)
    {
        _worker_local_heaps[worker_id].refund_remote_freed_memory();
    }

    /**
     * @return True, if all blocks of all numa regions are free.
     */
    [[nodiscard]] bool is_free() const noexcept;

private:
    /// Map from worker id to numa node id.
    std::array<std::uint8_t, tasking::config::max_cores()> _numa_node_ids;

    /// One heap for every worker.
    WorkerHeap *_worker_local_heaps;

    std::uint16_t _count_workers;
};

class CleanUpMemoryTask final : public tasking::TaskInterface
{
public:
    constexpr CleanUpMemoryTask(Allocator &allocator) noexcept : _allocator(allocator) {}
    ~CleanUpMemoryTask() noexcept override = default;

    tasking::TaskResult execute(const std::uint16_t worker_id) override
    {
        _allocator.clean_up_remote_freed_memory(worker_id);

        return tasking::TaskResult::make_remove();
    }

private:
    Allocator &_allocator;
};
} // namespace mx::memory::dynamic::local
#include "worker_local_dynamic_size_allocator.h"
#include "alignment_helper.h"
#include "global_heap.h"
#include <cassert>

using namespace mx::memory::dynamic::local;

std::tuple<std::set<AllocatedBlock::FreeHeaderDescriptor>::iterator, bool, std::size_t, std::size_t> AllocatedBlock::
    find_free_header(const std::size_t alignment, std::size_t size) const
{
    /// This is the minimal size we need to allocate,
    /// if and only if the address is perfectly aligned.
    /// However, we can filter out all free blocks with a
    /// size lower than that.
    const auto size_including_header = size + sizeof(AllocationHeader);

    for (auto iterator = this->_free_header.begin(); iterator != this->_free_header.end(); ++iterator)
    {
        const auto &descriptor = *iterator;

        if (descriptor.size() >= size_including_header)
        {
            const auto free_block_start_address = std::uintptr_t(descriptor.header());

            /// Calculate the number of bytes needed to fullfill the wanted aligntment for
            /// the given free block.
            /// The real allocation block will look like:
            ///    | (additional_size_to_fulfill_alignment[=0])(AllocationHeader)(size [+rest if free header has too
            ///    less left]) |
            const auto allocation_start_address = free_block_start_address + sizeof(AllocationHeader);
            const auto additional_size_to_fulfill_alignment = alignment - allocation_start_address % alignment;

            /// Check if the block has enough space to fullfill the alignment.
            const auto size_to_fulfill_alignment = size_including_header + additional_size_to_fulfill_alignment;
            if (descriptor.size() >= size_to_fulfill_alignment)
            {
                /// Check if we should split the block.
                const auto remaining_size = descriptor.size() - size_to_fulfill_alignment;

                /// The header will be take totally because the rest would be too small.
                /// We will increase the size (full free size - size for header - size before header for alignment).
                return std::make_tuple(iterator, remaining_size > 256U, size_including_header,
                                       additional_size_to_fulfill_alignment);
            }
        }
    }

    return std::make_tuple(this->_free_header.end(), false, 0U, 0U);
}

void *AllocatedBlock::allocate(const std::uint16_t worker_id, const std::uint8_t numa_node_id,
                               const std::size_t alignment, std::size_t size)
{
    const auto [iterator, is_split, size_including_header, additional_size_to_fulfill_alignment] =
        this->find_free_header(alignment, size);
    if (iterator == this->_free_header.end())
    {
        return nullptr;
    }

    auto *free_header = iterator->header();
    const auto free_header_size = iterator->size();
    auto next_iterator = this->_free_header.erase(iterator);

    const auto size_to_fulfill_alignment = size_including_header + additional_size_to_fulfill_alignment;

    if (is_split)
    {
        const auto remaining_size = free_header->size() - size_to_fulfill_alignment;
        auto *new_free_header = new (reinterpret_cast<void *>(std::uintptr_t(free_header) + size_to_fulfill_alignment))
            FreeHeader(remaining_size, numa_node_id, this->_id);

        this->_free_header.insert(next_iterator, FreeHeaderDescriptor{new_free_header, remaining_size});
    }
    else
    {
        size = free_header_size - sizeof(AllocationHeader) - additional_size_to_fulfill_alignment;
    }

    const auto header_address = std::uintptr_t(free_header) + additional_size_to_fulfill_alignment;
    auto *allocation_header = new (reinterpret_cast<void *>(header_address))
        AllocationHeader(size, additional_size_to_fulfill_alignment, worker_id, numa_node_id, this->_id);
    return allocation_header + 1U;
}

void AllocatedBlock::free(AllocationHeader *allocation_header)
{
    assert(this->_id == allocation_header->block_id());

    const auto ptr = std::uintptr_t(allocation_header) - allocation_header->unused_size_before_header();

    auto *free_header = new (reinterpret_cast<void *>(ptr)) FreeHeader(
        allocation_header->unused_size_before_header() + sizeof(AllocationHeader) + allocation_header->size(),
        allocation_header->numa_node_id(), this->_id);

    this->refund(free_header);
}

void AllocatedBlock::refund(FreeHeader *free_header)
{
    assert(std::uintptr_t(free_header) >= std::uintptr_t(this->_data));
    assert((std::uintptr_t(free_header) + free_header->size()) <= (std::uintptr_t(this->_data) + this->_size));

    auto descriptor = FreeHeaderDescriptor{free_header, free_header->size()};

    /// Try to merge with the descriptor next to the free one.
    auto next_descriptor = this->_free_header.upper_bound(descriptor);
    if (next_descriptor != this->_free_header.end())
    {
        if (free_header->is_right_neighbour(next_descriptor->header()))
        {
            descriptor.grow(next_descriptor->size());
            next_descriptor = this->_free_header.erase(next_descriptor);
        }
    }

    auto iterator = this->_free_header.insert(next_descriptor, descriptor);

    if (iterator != this->_free_header.begin())
    {
        auto inserted_iterator = iterator--;

        if (iterator->header()->is_right_neighbour(
                free_header)) /// Iterator is now the free header before inserted_iterator.
        {
            auto new_descriptor = *iterator;
            new_descriptor.grow(inserted_iterator->size());

            /// Remove the i-1 and i.
            this->_free_header.erase(inserted_iterator);
            auto next_iterator = this->_free_header.erase(iterator);

            /// Re-insert as a grown header.
            this->_free_header.insert(next_iterator, new_descriptor);
        }
    }
}

WorkerHeap::WorkerHeap(std::uint16_t worker_id, std::uint8_t numa_node_id)
    : _worker_id(worker_id), _numa_node_id(numa_node_id)
{
}

WorkerHeap::WorkerHeap(mx::memory::dynamic::local::WorkerHeap &&other) noexcept
    : _worker_id(other._worker_id), _numa_node_id(other._numa_node_id), _next_block_id(other._next_block_id)
{
    for (auto i = 0U; i < config::max_numa_nodes(); ++i)
    {
        this->_allocated_blocks[i] = std::move(other._allocated_blocks[i]);
        this->_allocated_block_indices[i] = std::move(other._allocated_block_indices[i]);

        FreeHeader *free_header;
        while ((free_header = other._remote_free_lists[i].pop_front()) != nullptr)
        {
            this->_remote_free_lists[i].push_back(free_header);
        }
    }
}

void *WorkerHeap::allocate(const std::uint8_t numa_node_id, const std::size_t alignment, const std::size_t size)
{
    /// (1) Check all blocks for free memory.
    auto &numa_blocks = this->_allocated_blocks[numa_node_id];
    for (auto i = std::int64_t(numa_blocks.size() - 1U); i >= 0; --i)
    {
        auto *allocated_block = numa_blocks[i].allocate(this->_worker_id, numa_node_id, alignment, size);
        if (allocated_block != nullptr)
        {
            return allocated_block;
        }
    }

    /// (2) Check free list from other cores for free memory.
    FreeHeader *header;
    while ((header = this->_remote_free_lists[numa_node_id].pop_front()) != nullptr)
    {
        header->next(nullptr);

        const auto qualifies = header->size() >= size;

        auto &index = this->_allocated_block_indices[header->numa_node_id()];
        if (auto iterator = index.find(header->block_id()); iterator != index.end())
        {
            auto &block = this->_allocated_blocks[header->numa_node_id()][iterator->second];
            assert(block.id() == header->block_id());
            block.refund(header);

            if (qualifies)
            {
                auto *allocation = block.allocate(this->_worker_id, numa_node_id, alignment, size);
                if (allocation != nullptr)
                {
                    return allocation;
                }
            }
        }
    }

    /// (3) Allocate a new block.
    const auto size_to_alloc_from_global_heap = std::max<std::size_t>(
        AllocatedBlock::DEFAULT_SIZE_IN_BYTES, alignment_helper::next_multiple(size + sizeof(AllocationHeader), 64UL));
    auto *data = GlobalHeap::allocate(numa_node_id, size_to_alloc_from_global_heap);
    auto &allocated_block = this->_allocated_blocks[numa_node_id].emplace_back(this->_next_block_id++, numa_node_id,
                                                                               size_to_alloc_from_global_heap, data);

    /// Update the index.
    this->_allocated_block_indices[numa_node_id].insert(
        std::make_pair(allocated_block.id(), this->_allocated_blocks[numa_node_id].size() - 1U));

    return allocated_block.allocate(this->_worker_id, numa_node_id, alignment, size);
}

void WorkerHeap::free(AllocationHeader *allocated_block)
{
    auto &index = this->_allocated_block_indices[allocated_block->numa_node_id()];
    if (auto iterator = index.find(allocated_block->block_id()); iterator != index.end())
    {
        this->_allocated_blocks[allocated_block->numa_node_id()][iterator->second].free(allocated_block);
        return;
    }
}

void WorkerHeap::release_free_memory()
{
    this->refund_remote_freed_memory();

    for (auto numa_node_id = 0U; numa_node_id < this->_allocated_blocks.size(); ++numa_node_id)
    {
        auto &allocated_blocks = this->_allocated_blocks[numa_node_id];

        allocated_blocks.erase(std::remove_if(allocated_blocks.begin(), allocated_blocks.end(),
                                              [](const auto &block) { return block.is_free(); }),
                               allocated_blocks.end());

        auto &index = this->_allocated_block_indices[numa_node_id];
        index.clear();
        for (auto i = 0UL; i < allocated_blocks.size(); ++i)
        {
            index.insert(std::make_pair(allocated_blocks[i].id(), i));
        }
    }
}

void WorkerHeap::release_all_memory()
{
    for (auto &allocated_blocks : this->_allocated_blocks)
    {
        allocated_blocks.clear();
    }

    for (auto &index : this->_allocated_block_indices)
    {
        index.clear();
    }
}

void WorkerHeap::refund_remote_freed_memory()
{
    for (auto i = 0U; i < this->_remote_free_lists.max_size(); ++i)
    {
        /// Check local numa region first.
        const auto nid = (this->_numa_node_id + i) & (this->_remote_free_lists.max_size() - 1U);
        FreeHeader *header;
        while ((header = this->_remote_free_lists[nid].pop_front()) != nullptr)
        {
            header->next(nullptr);

            auto &index = this->_allocated_block_indices[header->numa_node_id()];
            if (auto iterator = index.find(header->block_id()); iterator != index.end())
            {
                auto &block = this->_allocated_blocks[header->numa_node_id()][iterator->second];
                assert(block.id() == header->block_id());
                block.refund(header);
            }
        }
    }
}

void WorkerHeap::initialize(const std::uint8_t numa_nodes)
{
    for (auto numa_node_id = 0U; numa_node_id < numa_nodes; ++numa_node_id)
    {
        auto &index = this->_allocated_block_indices[numa_node_id];
        if (index.max_size() < 1024U)
        {
            index.reserve(1024U);
        }

        if (this->_allocated_blocks[numa_node_id].empty())
        {
            const auto size = AllocatedBlock::DEFAULT_SIZE_IN_BYTES *
                              (1U + (static_cast<std::uint8_t>(numa_node_id == this->_numa_node_id) * 3U));

            auto *data = GlobalHeap::allocate(numa_node_id, size);
            auto &block = this->_allocated_blocks[numa_node_id].emplace_back(this->_next_block_id++, numa_node_id, size, data);

            index.insert(std::make_pair(block.id(), this->_allocated_blocks[numa_node_id].size() - 1U));
        }
    }
}

bool WorkerHeap::is_free() const noexcept
{
    for (const auto &allocated_blocks : this->_allocated_blocks)
    {
        for (const auto &block : allocated_blocks)
        {
            if (block.is_free() == false)
            {
                return false;
            }
        }
    }

    return true;
}

Allocator::Allocator(const util::core_set &cores) : _count_workers(cores.count_cores())
{
    this->_worker_local_heaps = reinterpret_cast<WorkerHeap *>(
        GlobalHeap::allocate_cache_line_aligned(sizeof(WorkerHeap) * cores.count_cores()));
    for (auto i = std::uint16_t(0U); i < cores.count_cores(); ++i)
    {
        auto numa_node_id = cores.numa_node_id(i);
        new (reinterpret_cast<void *>(&this->_worker_local_heaps[i])) WorkerHeap(i, numa_node_id);
        this->_numa_node_ids[i] = numa_node_id;
    }
}

void Allocator::initialize_heap(const std::uint16_t worker_id, const std::uint8_t count_numa_nodes)
{
    this->_worker_local_heaps[worker_id].initialize(count_numa_nodes);
}

Allocator::~Allocator()
{
    for (auto i = 0U; i < this->_count_workers; ++i)
    {
        this->_worker_local_heaps[i].~WorkerHeap();
    }
    std::free(this->_worker_local_heaps);
}

void *Allocator::allocate(const std::uint16_t worker_id, const std::uint8_t numa_node_id, const std::size_t alignment,
                          const std::size_t size)
{
    return this->_worker_local_heaps[worker_id].allocate(numa_node_id, alignment, size);
}

void Allocator::free(const std::uint16_t calling_worker_id, void *pointer)
{
    auto *allocation_header = reinterpret_cast<AllocationHeader *>(std::uintptr_t(pointer) - sizeof(AllocationHeader));
    auto &heap = this->_worker_local_heaps[allocation_header->worker_id()];

    if (allocation_header->worker_id() == calling_worker_id)
    {
        heap.free(allocation_header);
    }
    else
    {
        heap.free(this->_numa_node_ids[calling_worker_id], allocation_header);
    }
}

void Allocator::free(void *pointer)
{
    auto *allocation_header = reinterpret_cast<AllocationHeader *>(std::uintptr_t(pointer) - sizeof(AllocationHeader));
    auto &heap = _worker_local_heaps[allocation_header->worker_id()];

    heap.free(system::cpu::node_id(), allocation_header);
}

void Allocator::reset(const util::core_set &cores, bool force_free_memory)
{
    if (force_free_memory)
    {
        for (auto i = 0U; i < this->_count_workers; ++i)
        {
            this->_worker_local_heaps[i].release_all_memory();
        }
    }
    else
    {
        for (auto i = 0U; i < this->_count_workers; ++i)
        {
            this->_worker_local_heaps[i].release_free_memory();
        }
    }

    if (this->_count_workers < cores.count_cores())
    {
        const auto old_count_workers = std::exchange(this->_count_workers, cores.count_cores());

        auto *old_local_worker_heaps = this->_worker_local_heaps;
        auto *new_local_worker_heaps = reinterpret_cast<WorkerHeap *>(
            GlobalHeap::allocate_cache_line_aligned(sizeof(WorkerHeap) * cores.count_cores()));

        /// Re-initialize old workers on new storage.
        auto worker_id = 0U;
        for (; worker_id < old_count_workers; ++worker_id)
        {
            const auto numa_node_id = cores.numa_node_id(worker_id);
            new (reinterpret_cast<void *>(&new_local_worker_heaps[worker_id]))
                WorkerHeap(std::move(old_local_worker_heaps[worker_id]));
            this->_numa_node_ids[worker_id] = numa_node_id;
        }

        /// Create new workers on new storage.
        for (; worker_id < cores.count_cores(); ++worker_id)
        {
            const auto numa_node_id = cores.numa_node_id(worker_id);
            new (reinterpret_cast<void *>(&new_local_worker_heaps[worker_id])) WorkerHeap(worker_id, numa_node_id);
            this->_numa_node_ids[worker_id] = numa_node_id;
        }

        /// Free old storage.
        std::free(old_local_worker_heaps);
        this->_worker_local_heaps = new_local_worker_heaps;
    }
}

bool Allocator::is_free() const noexcept
{
    for (auto i = 0U; i < this->_count_workers; ++i)
    {
        if (this->_worker_local_heaps[i].is_free() == false)
        {
            return false;
        }
    }

    return true;
}

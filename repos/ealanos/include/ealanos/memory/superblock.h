/**
 * @file superblock.h
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief Superblock definition
 * @version 0.1
 * @date 2025-03-06
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __INCLUDE__EALANOS__MEMORY__SUPERBLOCK_H_
#define __INCLUDE__EALANOS__MEMORY__SUPERBLOCK_H_

#include <ealanos/util/bit_alloc.h>
#include <base/stdint.h>
#include <base/ram_allocator.h>

namespace Ealan::Memory {
    class Block;
    class Hyperblock;
    template <int SIZE, unsigned BASE>
    class Superblock;
}

/**
 * @brief A memory area that may contain a superblock or user-data
 * 
 * @details A hyperblock represents an arbitrary chunk of memory that is backed by
 *          physical memory. In Genode terms, it is both a dataspace and a region.
 *          Hyperblocks may be chained together to build an unbounded memory allocator.
 * 
 */
class Ealan::Memory::Hyperblock
{
    public:
        Hyperblock *_next{nullptr};
        Genode::Ram_dataspace_capability cap{};
        void *operator new(Genode::size_t, void *p) { return p; }
        Hyperblock *next() { return _next; }
        void next(Hyperblock *p) { _next = p; }
};

/**
 * @brief A block of memory
 * 
 */
struct Ealan::Memory::Block
{
    void *_superblock{nullptr}; /* Pointer to the superblock, this block was allocated from. */
    char _padding[56];

    /**
        * @brief Return a pointer to the metadata of this block
        * 
        * @param ptr - application-facing pointer for which to request the metadata
        * @return Block* - pointer to the Block datastructure containing the metadata
        */
    static Block *metadata(void *ptr) {
        return reinterpret_cast<Block *>(reinterpret_cast<Genode::addr_t>(ptr) - sizeof(Block));
    }

    bool reserve(void *sb) {
        void *expect{nullptr};
        return __atomic_compare_exchange_n(&_superblock, &expect, sb, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
    }

    void free() {
        __atomic_store_n(&_superblock, nullptr, __ATOMIC_RELEASE);
    }
};

/**
 * @brief A memory area containing consecutive equally sized blocks of memory.
 * 
 * @details A superblock is a waitfree bounded heap of equally sized blocks of memory. Each 
 *          superblock is also a hyperblock and contains at most SIZE/BASE blocks.
 * 
 * @tparam SIZE - size of this superblock in bytes, used to calculate number of blocks
 *                this superblock can supply
 * @tparam BASE - size of the underlying block structure
 */
template <int SIZE, unsigned BASE>
class Ealan::Memory::Superblock : public Hyperblock
{
    private:
        Genode::size_t _size_class;
        alignas(64) Genode::addr_t _start{0}; /* Start address of the blocks */

    public:
        Superblock(Genode::size_t sz) : _size_class(sz)
        {
            if (_size_class > SIZE) {
                Genode::error("Size class ", _size_class, " is bigger than superblock size ", SIZE);
            }
            Genode::log("Superblock SIZE=", SIZE, " BASE=", BASE, " this at ", this);
            Genode::log("Block metadata size is ", sizeof(Block));
            Genode::log("Size class of superblock is ", _size_class);
            Block *end = reinterpret_cast<Block *>(reinterpret_cast<Genode::addr_t>(this) + SIZE);
            Genode::log("Superblock ends at ", end);
            Genode::log("Capacity is ", capacity());
            Genode::log("-------------------");
        }

        /**
         * @brief Allocate a block of SIZE-8 bytes from this superblock
         * 
         * @return void* - pointer to the allocated block
         */
        void *alloc() {
            Block *block = reinterpret_cast<Block *>(&_start);
            Block *end = reinterpret_cast<Block *>(reinterpret_cast<Genode::addr_t>(this) + SIZE - 64);
            while (block < end) {
                if (block->_superblock == nullptr) {
                    if (block->reserve(this))
                    {
                        return reinterpret_cast<void*>(reinterpret_cast<Genode::addr_t>(block)+64);
                    }
                }
                Genode::addr_t next = reinterpret_cast<Genode::addr_t>(block) + sizeof(Block) + _size_class;
                block = reinterpret_cast<Block *>(next);
            }
            return nullptr;
        }

        /**
         * @brief Allocate a block from this superblock aligned to alignment
         * 
         * @param alignment - adress alignment to use (must be at least 8)
         * @return void* - pointer to this block aligned to alignment bytes boundary
         */
        void *aligned_alloc(Genode::size_t alignment = 0) {
            void *ptr = alloc();
            if (!ptr)
                return nullptr;
            return reinterpret_cast<void *>(reinterpret_cast<Genode::addr_t>(ptr) + alignment);
        }

        Genode::size_t capacity() {
            return (reinterpret_cast<Genode::addr_t>(this) + SIZE - reinterpret_cast<Genode::addr_t>(&_start)) / (sizeof(Block) + _size_class);
        }

        /**
         * @brief Free the block pointed to by ptr.
         * 
         * @details Frees the block pointed to by ptr and, thus, makes it
         *          available to the superblocks allocator again for reuse.
         *          The superblock field of the block _must_ not be overwritten
         *          as it is used to identify the right bit of it in the bitvector
         *          used by the superblocks allocator.
         * 
         * @param ptr - Pointer to memory block to free
         */
        void free(void *ptr) {
            if (!ptr)
                return;

            Block *b = reinterpret_cast<Block *>(reinterpret_cast<Genode::addr_t>(ptr) - 64);
            Block *end = reinterpret_cast<Block *>(reinterpret_cast<Genode::addr_t>(this) + SIZE);
            if (b > --end)
                return;

            b->free();
        }

        /**
         * @brief Return the address of the first memory block in this superblock
         * 
         * @return Block* - address of the first block
         */
        Block *start() { return reinterpret_cast<Block*>(&_start); }

        /**
         * @brief Placement new used to create a superblock at address pointed to by p.
         * 
         * @param p - Pointer to address to create superblock at.
         * @return void* - Pointer to the new superblock.
         */
        void *operator new(Genode::size_t, void *p) { return p; }

        Superblock<SIZE, BASE> *next() { return static_cast<Superblock<SIZE, BASE> *>(_next); }

        void next(Superblock<SIZE, BASE> *sb) { _next = static_cast<Hyperblock *>(sb); }
};

#endif /* __INCLUDE__EALANOS__MEMORY__SUPERBLOCK_H_ */
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
class Ealan::Memory::Block
{
    public:
        void *_superblock{nullptr}; /* Pointer to the superblock, this block was allocated from. */
        void *_block_start; /* Placeholder for marking the start of the usable area of this block. */

        /**
         * @brief Construct a new Block object
         * 
         * @param sb - Pointer to the superblock this block was allocated from
         */
        Block(void *sb) : _superblock(sb), _block_start{nullptr} {}

        /**
         * @brief Creates a new block at the address pointed to by p
         * 
         * @param p - the address at which the block shall be created
         * @return void* - pointer to the new block
         */
        void *operator new(Genode::size_t, void *p) { return p; }
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
        Genode::size_t _size_class{0}; /* actual size of the blocks in this superblock */
        Bit_alloc<0, SIZE/BASE> _alloc; /* Bit allocator to allocate blocks from this superblock */
        alignas(BASE) Block _block_start[]; /* Marker for the end of the metadata portion */

    public:
        Superblock(Genode::size_t sz) : _size_class(sz), _alloc(SIZE / _size_class)
        {
        }

        /**
         * @brief Allocate a block of SIZE-8 bytes from this superblock
         * 
         * @return void* - pointer to the allocated block
         */
        void *alloc() {
            long idx = static_cast<long>(_alloc.alloc());
            
            if (!idx)
                return nullptr;

            Genode::addr_t block_addr = reinterpret_cast<Genode::addr_t>(_block_start) + idx * _size_class;
            Block *block = new (reinterpret_cast<void *>(block_addr)) Block(this);
            return &block->_block_start;
        }

        /**
         * @brief Allocate a block from this superblock aligned to alignment
         * 
         * @param alignment - adress alignment to use (must be at least 8)
         * @return void* - pointer to this block aligned to alignment bytes boundary
         */
        void *aligned_alloc(Genode::size_t alignment = 64) {
            void *ptr = alloc();
            Genode::addr_t base_ptr = reinterpret_cast<Genode::addr_t>(ptr) - sizeof(void *);
            return reinterpret_cast<void *>(base_ptr + alignment);
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
            Genode::size_t idx = (reinterpret_cast<Genode::size_t>(ptr) - reinterpret_cast<Genode::size_t>(_block_start)) / _size_class;
            _alloc.release(idx);
        }

        /**
         * @brief Return number of blocks that can be allocated from this superblock
         * 
         * @return Genode::size_t - Number of available blocks
         */
        Genode::size_t free_blocks() { return _alloc.left(); }

        /**
         * @brief Return the address of the first memory block in this superblock
         * 
         * @return Block* - address of the first block
         */
        Block *start() { return _block_start; }

        /**
         * @brief Placement new used to create a superblock at address pointed to by p.
         * 
         * @param p - Pointer to address to create superblock at.
         * @return void* - Pointer to the new superblock.
         */
        void *operator new(Genode::size_t, void *p) { return p; }
};

#endif /* __INCLUDE__EALANOS__MEMORY__SUPERBLOCK_H_ */
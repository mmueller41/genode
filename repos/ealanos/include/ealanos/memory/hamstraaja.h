/**
 * @file hamstraaja.h
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief Hamstraaja - a lock-free, dynamic, NUMA-aware memory allocator
 * @version 0.1
 * @date 2025-04-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __INCLUDE__EALANOS__MEMORY__HAMSTRAAJA_H_
#define __INCLUDE__EALANOS__MEMORY__HAMSTRAAJA_H_

#include <ealanos/memory/coreheap.h>
#include <tukija/syscall-generic.h>
#include <base/affinity.h>
#include <base/heap.h>
#include <base/thread.h>

namespace Ealan::Memory {
    template <unsigned MIN, unsigned MAX>
    class Hamstraaja;
}

/**
 * @brief Hamstraaja - a lock-free, dynamic, NUMA-aware allocator
 * 
 * @tparam MIN - minimum size of a memory block
 * @tparam MAX - maximum size of a memory block, chunks greater than this value will be allocated directly from Genode's page allocator
 */
template <unsigned MIN, unsigned MAX>
class Ealan::Memory::Hamstraaja : public Genode::Allocator
{
    private:
        using Heap = Core_heap<MIN, MAX>;

        Pd_session &_pd;
        Region_map &_rm;
        Heap *_core_heaps[256];
        Genode::Heap _backend{_pd, _rm};
        size_t _quota_used{0};

        Heap &_location_to_heap(Affinity::Location loc) const
        {
            size_t pos = loc.xpos() * loc.height() + loc.ypos();
            if (!_core_heaps[pos]) {
                Genode::error("No heap for location ", loc);
            }
            return *_core_heaps[pos];
        }

        Affinity::Location _my_location() const
        {
            return Thread::myself()->affinity();
        }

        Hamstraaja &operator=(Hamstraaja &copy);
        Hamstraaja(Hamstraaja const &copy);

    public:
        Hamstraaja(Genode::Pd_session &pd, Genode::Region_map &rm) : _pd(pd), _rm(rm) 
        {
            size_t num_cpus = Cip::cip()->habitat_affinity.total();
            for (size_t cpu = 0; cpu < num_cpus; cpu++) {
                _core_heaps[cpu] = new (_backend) Core_heap<MIN, MAX>(_pd, _rm);
                Genode::log("Size of CoreHeap for size ", MAX * 2, " with ", MIN, " blocks is: ", sizeof(Core_heap<MIN, MAX>));
            }
        }

        ~Hamstraaja() 
        {
            size_t num_cpus = Cip::cip()->habitat_affinity.total();
            for (size_t cpu = 0; cpu < num_cpus; cpu++) {
                Genode::destroy(_backend, _core_heaps[cpu]);
            }
        }

        /**
         * @brief Allocate a chunk of memory of size 'size'.
         * 
         * @param size - amount of memory to allocate
         * @param alignment - alignment of the memory chunk to allocate
         * @param domain_id - NUMA node from which to allocate
         * @return void* - pointer to allocated memory chunk
         */
        void *aligned_alloc(size_t size, size_t alignment, unsigned domain_id)
        {
            _quota_used += overhead(size) + size;
            return _location_to_heap(_my_location()).aligned_alloc(size, domain_id, alignment);
        }

        /**
         * @brief Allocate a chunk of memory from a NUMA domain without alignment
         * 
         * @param size - amount of memory to allocate
         * @param domain_id - NUMA node from which to allocate
         * @return void* - pointer to allocated memory
         */
        void *alloc(size_t size, unsigned domain_id)
        {
            _quota_used += overhead(size) + size;
            return _location_to_heap(_my_location()).aligned_alloc(size, 0, domain_id);
        }

        /**
         * @brief Allocate memory regardless from local NUMA node regardless the alignment
         * 
         * @param size - size of the desired memory chunk
         * @return void* - pointer to the memory chunk
         */
        void *alloc(size_t size) 
        {
            _quota_used += overhead(size) + size;
            //Genode::log("Allocating ", size, " bytes.");
            return _location_to_heap(_my_location()).alloc(size);
        }

        /**
         * @brief Free a chunk of memory
         * 
         * @param ptr - pointer to the memory chunk
         * @param alignment - alignment that was used for allocating
         * @param size - size of the chunk (only used for accounting)
         */
        void free(void *ptr, size_t alignment, size_t size)
        {
            if (!ptr) {
                Genode::warning("Tried to free nullptr");
                return;
            }
            _quota_used -= overhead(size) + size;
            return _location_to_heap(_my_location()).free(ptr, alignment);
        }

        /**
         * @brief Reserve a set of superblocks for a given size class and from a given NUMA node
         * 
         * @param count - number of superblocks to allocate
         * @param sz_class - desired size class of the superblocks
         * @param domain_id - NUMA node from which to take the superblocks
         */
        void reserve_superblocks(size_t count, size_t sz_class, unsigned domain_id)
        {
            _location_to_heap(_my_location()).reserve_superblocks(count, domain_id, sz_class);
        }

        void reserve_superblocks_for_location(Affinity::Location loc, size_t count, size_t sz_class, unsigned domain_id)
        {
            _location_to_heap(loc).reserve_superblocks(count, domain_id, sz_class);
        }
		
        /*************************
		 ** Allocator interface **
		 *************************/

		Alloc_result try_alloc(size_t size)          override
        {
            void *ptr = nullptr;
            if ((ptr = alloc(size)) != nullptr)
            {
                return Alloc_result(ptr);
            }
            else
            {
                return Alloc_result(Alloc_error::OUT_OF_RAM);
            }
        }

        void free(void *ptr, size_t size=0) override { free(ptr, 0, size); }
        size_t       consumed()            const override { return _quota_used; }
		size_t       overhead(size_t size) const override 
        {
            Heap &heap = _location_to_heap(_my_location());
            return heap._calculate_size_class(size);
        }

        bool         need_size_for_free()  const override { return false; }
};

#endif /* __INCLUDE__EALANOS__MEMORY__HAMSTRAAJA_H_ */
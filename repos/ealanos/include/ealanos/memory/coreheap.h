/**
 * @file coreheap.h
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief CPU core-local heap of superblocks
 * @version 0.1
 * @date 2025-04-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __INCLUDE__EALANOS__MEMORY__COREHEAP_H_
#define __INCLUDE__EALANOS__MEMORY__COREHEAP_H_

#include <ealanos/memory/superblock.h>
#include <ealanos/util/lifo_queue.h>
#include <tukija/syscall-generic.h>
#include <base/attached_ram_dataspace.h>
#include <pd_session/pd_session.h>
#include <region_map/region_map.h>

namespace Ealan::Memory {
    template <unsigned MIN, unsigned MAX>
    class Core_heap;
    template <unsigned MIN, unsigned MAX>
    class Hamstraaja;
    using namespace Tukija;
    using namespace Genode;
}

template <unsigned MIN, unsigned MAX>
class Ealan::Memory::Core_heap
{
    using Sb = Superblock<MAX * 2, MIN>;
    friend class Hamstraaja<MIN, MAX>;

    private:
        static constexpr const Genode::size_t num_size_classes = MAX / MIN;
        static constexpr const unsigned num_numa_domains = 64;
        static constexpr const unsigned long magic_num = 0xdeadbeefUL;
        alignas(64) Ealan::util::Lifo_queue<Sb> _superblocks[num_size_classes][num_numa_domains];
        Pd_session &_pd;
        Region_map &_rm;
        Tip *_tip{const_cast<Tip *>(Tip::tip())};
        Cip *_cip{Cip::cip()};

        Genode::size_t _calculate_size_class(Genode::size_t size) const
        {
            return (size / MIN + 1) * MIN;
        }

        Hyperblock *_allocate_hyperblock(unsigned domain_id, Genode::size_t size)
        {
            Tukija::uint8_t mem_regions = 0;
            _tip = const_cast<Tip *>(Tip::tip());
			try {
                Tip::Memory_region &region = _tip->memory_for_domain(domain_id, &mem_regions);
                Range_allocator::Range range = {.start = reinterpret_cast<addr_t>(region.start), .end = reinterpret_cast<addr_t>(region.end)};

                Ram_dataspace_capability ds_cap = _pd.try_alloc_from_range(size, Genode::CACHED, range).convert<Ram_dataspace_capability>([&](Ram_dataspace_capability cap)
                                                                                                                                        { return cap; },
                                                                                                                                        [&](Ram_allocator::Alloc_error err)
                                                                                                                                        { 
                        Genode::error("Failed to allocate phyiscal memory in domain ", domain_id, ":", err, " avail quota: ", _pd.avail_ram());
                        return Ram_dataspace_capability(); });

                if (!ds_cap.valid()) {
                    Genode::warning("Failed to allocate physical memory for hyperblock");
                    return nullptr;
                }

                Region_map::Attr attr{};
                attr.writeable = true;
                void *hb = _rm.attach(ds_cap, attr).convert<void *>(
                    [&](Region_map::Range r) { return reinterpret_cast<void *>(r.start); },
                    [&](Region_map::Attach_error) { return nullptr; });

                if (!hb) {
                    Genode::warning("Failed to obtain regionmap for hyperblock");
                    return nullptr;
                }

                Hyperblock *hyperblock = new (hb) Hyperblock();
                hyperblock->cap = ds_cap;

                return hyperblock;
            } catch (Tukija::Tip::Domain_has_no_memory_regions) {
                Genode::error("Domain ", domain_id, " has no memory regions.");
                return nullptr;
            }
        }

        Sb *_allocate_superblock(unsigned domain_id, Genode::size_t sz_class)
        {
			Hyperblock *hb = _allocate_hyperblock(domain_id, MAX * 2);
            if (!hb) {
                Genode::warning("Failed to allocate superblock for size class ", sz_class, " in domain ", domain_id);
                return nullptr;
            }
            return new (static_cast<void *>(hb)) Superblock<MAX * 2, MIN>(sz_class);
        }

        Core_heap(Core_heap &);
        Core_heap &operator=(Core_heap &);

    public:
        Core_heap(Pd_session &pd, Region_map &rm) : _pd(pd), _rm(rm) {
        }

        ~Core_heap()
        {
            for (size_t sz_class = 0; sz_class < num_size_classes; sz_class++) {
                for (unsigned domain_id = 0; domain_id < num_numa_domains; domain_id++) {
                    Sb *sb;
                    while ((sb = _superblocks[sz_class][domain_id].dequeue()) != nullptr)
                    {
                        Ram_dataspace_capability cap = sb->cap;
                        _rm.detach(reinterpret_cast<addr_t>(sb));
                        _pd.free(cap);
                    }
                }
            }
        }

        void *aligned_alloc(Genode::size_t size, unsigned domain_id, Genode::size_t alignment)
        {
            void *ptr = nullptr;

            if (size > MAX)
            {
                /* directly allocate a hyperblock */
				Hyperblock *hb = _allocate_hyperblock(
					domain_id, size + sizeof(Hyperblock *) + sizeof(Ram_dataspace_capability));
				if (!hb) {
					Genode::warning("Failed to allocate hyperblock of size ", size);
					return nullptr;
				}
                hb->_next = reinterpret_cast<Hyperblock*>(magic_num);
                return reinterpret_cast<char *>(hb) + sizeof(Hyperblock *) + sizeof(Ram_dataspace_capability);
            }

            Genode::size_t sz_class = _calculate_size_class(size+alignment);
            Sb *sb = _superblocks[sz_class / MIN - 1][domain_id].head();

			//if (size == 128)
			  //  Genode::log("Allocating task object from superblock ", sb, " sz_class=", sz_class, " node_id = ", domain_id);

			if (!sb) {
                sb = _allocate_superblock(domain_id, sz_class);
                if (!sb)
                    return nullptr;
                _superblocks[sz_class / MIN - 1][domain_id].enqueue(sb);
            } 
            
            for (; sb != nullptr ; sb = static_cast<Sb*>(sb->next())) {
                ptr = sb->aligned_alloc(alignment);
                if (ptr)
                    return ptr;
            }
            
            if (!sb) {
                sb = _allocate_superblock(domain_id, sz_class);
                if (!sb)
                    return nullptr;
                _superblocks[sz_class / MIN - 1][domain_id].enqueue(sb);
                return sb->aligned_alloc(alignment);
            }

            return ptr;
        }

        void *aligned_alloc(Genode::size_t size, Genode::size_t alignment)
        {
            unsigned cpu = _cip->location_to_kernel_cpu(Thread::myself()->affinity());
            unsigned domain_id = _tip->cpu_to_domain[cpu];

            return aligned_alloc(size, domain_id, alignment);
        }

        void *alloc(Genode::size_t size, unsigned domain_id) 
        {
            return aligned_alloc(size, domain_id, 0);
        }

        void *alloc(Genode::size_t size)
        {
            return aligned_alloc(size, 0);
        }

        void free(void *ptr, Genode::size_t alignment = 0)
		{
            if (!ptr) {
                Genode::warning("Tried to free nullptr");
                return;
            }
            void *p = reinterpret_cast<void *>((reinterpret_cast<addr_t>(ptr) - sizeof(Hyperblock *) - sizeof(Ram_dataspace_capability)));
            Hyperblock *hb = reinterpret_cast<Hyperblock *>(p);

            if (reinterpret_cast<unsigned long>(hb->_next) == magic_num) {
                Ram_dataspace_capability cap = hb->cap;
                _rm.detach(reinterpret_cast<addr_t>(p));
                _pd.free(cap);
                return;
            }

            p = reinterpret_cast<void*>(reinterpret_cast<Genode::addr_t>(ptr) - alignment);

            Block *b = Block::metadata(p);

            if (!b) {
                Genode::warning("Invalid block. Not freeing.");
                return;
            }
            Sb *sb = static_cast<Sb*>(b->_superblock);
            if (!sb) {
                Genode::warning("Corrupt or invalid memory block: ", p);
                return;
            }
            sb->free(p);
        }

        void reserve_superblocks(size_t count, unsigned domain_id, size_t sz_class)
        {
            for (size_t i = 0; i < count; i++) {
                Sb *sb = _allocate_superblock(domain_id, sz_class);
                _superblocks[sz_class / MIN - 1][domain_id].enqueue(sb);
            }
        }
};

#endif // __INCLUDE__EALANOS__MEMORY__COREHEAP_H_

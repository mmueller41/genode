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
#include <ealanos/util/mpsc_queue.h>
#include <tukija/syscall-generic.h>
#include <base/attached_ram_dataspace.h>
#include <pd_session/pd_session.h>
#include <region_map/region_map.h>

namespace Ealan::Memory {
    template <unsigned MIN, unsigned MAX>
    class Core_heap;
    using namespace Tukija;
    using namespace Genode;
}

template <unsigned MIN, unsigned MAX>
class Ealan::Memory::Core_heap
{
    using Sb = Superblock<MAX * 2, MIN>;
    
    private:
        static constexpr const Genode::size_t num_size_classes = MAX / MIN;
        static constexpr const unsigned long magic_num = 0xdeadbeefUL;
        Ealan::util::MPSCQueue<Sb> _superblocks[num_size_classes];
        Pd_session &_pd;
        Region_map &_rm;
        Tip *_tip{const_cast<Tip*>(Tip::tip())};
        Cip *_cip{Cip::cip()};

        Genode::size_t _calculate_size_class(Genode::size_t size) {
            return (size / MIN + 1) * MIN;
        }

        Hyperblock *_allocate_hyperblock(unsigned domain_id, Genode::size_t size)
        {
            Tukija::uint8_t mem_regions = 0;
            _tip = const_cast<Tip *>(Tip::tip());
            Tip::Memory_region &region = _tip->memory_for_domain(domain_id, &mem_regions);
            Range_allocator::Range range = {.start = reinterpret_cast<addr_t>(region.start), .end = reinterpret_cast<addr_t>(region.end)};

            Ram_dataspace_capability ds_cap = _pd.try_alloc_from_range(size, Genode::CACHED, range).convert<Ram_dataspace_capability>(
                [&](Ram_dataspace_capability cap) { return cap; },
                [&](Ram_allocator::Alloc_error) { return Ram_dataspace_capability(); });

            if (!ds_cap.valid())
                return nullptr;

            Region_map::Attr attr{};
            attr.writeable = true;
            void *hb = _rm.attach(ds_cap, attr).convert<void *>(
                [&](Region_map::Range r) { return reinterpret_cast<void *>(r.start); },
                [&](Region_map::Attach_error) { return nullptr; });

            if (!hb)
                return nullptr;

            Hyperblock *hyperblock = new (hb) Hyperblock();
            hyperblock->cap = ds_cap;

            return hyperblock;
        }

        Sb *_allocate_superblock(unsigned domain_id, Genode::size_t sz_class)
        {
            Hyperblock *hb = _allocate_hyperblock(domain_id, MAX * 2);
            return new (static_cast<void *>(hb)) Superblock<MAX * 2, MIN>(sz_class);
        }

    public:
        Core_heap(Pd_session &pd, Region_map &rm) : _pd(pd), _rm(rm) {}

        void *aligned_alloc(Genode::size_t size, unsigned domain_id, Genode::size_t alignment)
        {
            if (size > MAX) {
                /* directly allocate a hyperblock */
                Hyperblock *hb = _allocate_hyperblock(domain_id, size+sizeof(Hyperblock*) + sizeof(Ram_dataspace_capability));
                hb->_next = reinterpret_cast<Hyperblock*>(magic_num);
                return reinterpret_cast<char *>(hb) + sizeof(Hyperblock *) + sizeof(Ram_dataspace_capability);
            }

            Genode::size_t sz_class = _calculate_size_class(size);
            Sb *sb = _superblocks[sz_class / MIN - 1].head();

            if (!sb) {
                sb = _allocate_superblock(domain_id, sz_class);
                _superblocks[sz_class / MIN - 1].push_back(sb);
            } else if (sb->free_blocks() == 0) {
                for (; sb && sb->free_blocks() == 0; sb = static_cast<Sb*>(sb->next()))
                    ;
                if (!sb) {
                    Sb *new_sb = _allocate_superblock(domain_id, sz_class);
                    _superblocks[sz_class / MIN - 1].push_back(new_sb);
                    sb = new_sb;
                }
            }

            return sb->aligned_alloc(alignment);
        }

        void *aligned_alloc(Genode::size_t size, Genode::size_t alignment)
        {
            unsigned cpu = _cip->location_to_kernel_cpu(Thread::myself()->affinity());
            unsigned domain_id = _tip->dom_of_cpu(cpu).id;

            return aligned_alloc(size, domain_id, alignment);
        }

        void *alloc(Genode::size_t size, unsigned domain_id) 
        {
            return aligned_alloc(size, domain_id, 64);
        }

        void *alloc(Genode::size_t size)
        {
            return aligned_alloc(size, 64);
        }

        void free(void *ptr)
        {
            void *p = (ptr - sizeof(Hyperblock *) - sizeof(Ram_dataspace_capability));
            Hyperblock *hb = reinterpret_cast<Hyperblock *>(p);

            if (reinterpret_cast<unsigned long>(hb->_next) == magic_num) {
                Ram_dataspace_capability cap = hb->cap;
                _rm.detach(reinterpret_cast<addr_t>(p));
                _pd.free(cap);
                return;
            }

            p = (ptr - sizeof(void *));
            Block *b = reinterpret_cast<Block *>(p);

            Sb *sb = b->_superblock;
            sb->free(ptr);
        }
};

#endif // __INCLUDE__EALANOS__MEMORY__COREHEAP_H_

/**
 * @file main.cc
 * @author your name (you@domain.com)
 * @brief Component for testing CPU core-local heaps
 * @version 0.1
 * @date 2025-04-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <ealanos/memory/coreheap.h>
#include <base/internal/xoroshiro.h>
#include <base/tslab.h>

#include <ealanos/memory/hamstraaja.h>
#include <tukija/syscall-generic.h>

namespace Ealan::Memory {
    class CoreheapTest;
    using namespace Genode;
}

class Ealan::Memory::CoreheapTest
{
    private:
        static constexpr const size_t MAX = 1024*1680;
        static constexpr const size_t MIN = 1680;
        static constexpr const size_t num_size_classes = MAX / MIN;

        Genode::Xoroshiro_128_plus random{42};

        Env &_env;
        Core_heap<MIN, MAX> *_heap{nullptr};
        Genode::Heap _genode_heap{_env.ram(), _env.rm()};
        Genode::Tslab<void*, 1024*1600> _tslab{_genode_heap};

    public:

        CoreheapTest(Env &env) : _env(env)
        {
            log("Starting tests for Core_heap");

            _heap = new (_genode_heap) Core_heap<MIN, MAX>(_env.pd(), _env.rm());
            Genode::log("Trying to allocate from all possible size classes");
            for (size_t sz = MIN; sz < num_size_classes*MIN; sz+=MIN) {
                size_t size = random.value() % sz;
                void *ptr = _heap->aligned_alloc(size, 1, 16);
                Genode::log("Allocated ", size, " bytes of sizeclass ", sz, " at ", ptr);
            }

            Genode::log("Exhausting super block of size class ", MIN);

            _heap->reserve_superblocks(32, 1, MIN);

            [[maybe_unused]] void *ptrs[2 * MAX / MIN];
            Genode::Trace::Timestamp start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++)
            {
                 ptrs[i] = _heap->aligned_alloc(1600, 1, 0);
            }
            Genode::Trace::Timestamp end = Genode::Trace::timestamp();

            Genode::log("Took ", (end - start), " cycles to allocate ", 2*MAX / MIN, " blocks from same superblock");

            [[maybe_unused]] void *gen_ptr = _genode_heap.alloc(MIN);
            [[maybe_unused]] void *genode_ptrs[2 * MAX / MIN];
            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++)
            {
                genode_ptrs[i] = _genode_heap.alloc(1600);
            }
            end = Genode::Trace::timestamp();
            Genode::log("Took ", (end - start), " cycles to allocate ", 2*MAX / MIN, " blocks from Genode::Heap");

            Genode::log("Freeing all blocks");
            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++) {
                _heap->free(ptrs[i], 0);
            }
            end = Genode::Trace::timestamp();
            Genode::log("Took ", (end - start), " cycles to free ", 2*MAX / MIN, " blocks from Core_heap");

            Genode::log("Freeing all blocks");
            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++) {
                _genode_heap.free(genode_ptrs[i], 1600);
            }
            end = Genode::Trace::timestamp();
            Genode::log("Took ", (end - start), " cycles to free ", 2*MAX / MIN, " blocks from Genode::Heap");

            
            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++)
            {
                 ptrs[i] = _heap->aligned_alloc(1600, 1, 0);
            }
            end = Genode::Trace::timestamp();

            Genode::log("Took ", (end - start), " cycles to allocate ", 2*MAX / MIN, " blocks from same superblock");

            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++)
            {
                genode_ptrs[i] = _tslab.alloc(1);
            }
            end = Genode::Trace::timestamp();
            Genode::log("Took ", (end - start), " cycles to allocate ", 2*MAX / MIN, " blocks from Genode::Tslab");
            
            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++) {
                _tslab.free(genode_ptrs[i], 1);
            }
            end = Genode::Trace::timestamp();
            Genode::log("Took ", (end - start), " cycles to free ", 2*MAX / MIN, " blocks from Genode::Tslab");


            Genode::log("Now, trying to allocate of size class 64, again");
            [[maybe_unused]] void *ptr = _heap->aligned_alloc(32, 1, 16);
            Genode::log("New block was allocated at ", ptr);
            _heap->free(ptr, 16);

            Genode::log("Allocating a hyperblock");
            ptr = _heap->aligned_alloc(3198, 2, 0);
            Genode::log("Hyperblock is at ", ptr);

            Genode::log("Freeing hyperblock");
            _heap->free(ptr);

            Genode::log("-------- Testing Hamstraaja --------");
            Hamstraaja<MIN, MAX> *_hamstraaja = new (_genode_heap) Hamstraaja<MIN, MAX>(_env.pd(), _env.rm());

            _hamstraaja->reserve_superblocks(32, MIN, 2);

            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++)
            {
                 ptrs[i] = _hamstraaja->aligned_alloc(1600, 0, 2);
            }
            end = Genode::Trace::timestamp();

            Genode::log("Took ", (end - start), " cycles to allocate ", 2*MAX / MIN, " blocks from Hamstraaja");
            
            start = Genode::Trace::timestamp();
            for (unsigned i = 0; i < 2*MAX / MIN; i++) {
                _hamstraaja->free(ptrs[i], 0);
            }
			end = Genode::Trace::timestamp();

			Genode::log("Trying to get memory from each NUMA region");
			Tukija::Tip::tip()->for_each([&](Tukija::Tip::Domain &dom) {
				void *ptr = _hamstraaja->aligned_alloc(64, 0, dom.id);
				Genode::log("[node ", dom.id, "] ", ptr);
			});
			
            Genode::log("Took ", (end - start), " cycles to free ", 2*MAX / MIN, " blocks from Hamstraaja");
            Genode::log("Testing Hamstraaja as drop-in replacement for Genode::Heap");

            Genode::Xml_node *xml_node = new (_hamstraaja) Xml_node("<test/>");

            Genode::log("Allocated XML node is at ", xml_node);

            Genode::destroy(_hamstraaja, xml_node);
        }
};

void Component::construct(Genode::Env &env)
{
    static Genode::Heap heap{env.ram(), env.rm()};
    [[maybe_unused]] Ealan::Memory::CoreheapTest *test = new (heap) Ealan::Memory::CoreheapTest(env);

}
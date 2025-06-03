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

#include "base/affinity.h"
#include "base/stdint.h"
#include "base/thread.h"
#include "trace/timestamp.h"
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
        static constexpr const size_t MAX = 1024*64;
        static constexpr const size_t MIN = 64;
        static constexpr const size_t num_size_classes = MAX / MIN;

        Genode::Xoroshiro_128_plus random{42};

        Env &_env;
        Core_heap<MIN, MAX> *_heap{nullptr};
        Genode::Heap _genode_heap{_env.ram(), _env.rm()};
        Genode::Tslab<void*, 1024*1600> _tslab{_genode_heap};

    public:

        CoreheapTest(Env &env) : _env(env)
        {
            [[maybe_unused]] void *ptrs[4 * MAX / MIN];
            Genode::log("-------- Testing Hamstraaja --------");
            Hamstraaja<MIN, MAX> *_hamstraaja = new (_genode_heap) Hamstraaja<MIN, MAX>(_env.pd(), _env.rm());

			Genode::Affinity::Location loc = Genode::Thread::myself()->affinity();
			for (unsigned node_id = 0; node_id < Tukija::Tip::tip()->num_domains(); node_id++) {
                _hamstraaja->reserve_superblocks_for_location(loc, 32, 192, node_id);
			}
            //_hamstraaja->reserve_superblocks(32, MIN, 2);

			for (int j = 0; j < 3; j++) {
                Genode::Trace::Timestamp start = Genode::Trace::timestamp();
                for (unsigned i = 0; i < 500; i++)
                {
					ptrs[i] = _hamstraaja->aligned_alloc(128, 0);
					if (reinterpret_cast<Genode::addr_t>(ptrs[i]) % 64 != 0) {
						Genode::error("Alignment error: ", ptrs[i]);
					}
				}
                Genode::Trace::Timestamp end = Genode::Trace::timestamp();

                Genode::log("Took ", (end - start), " cycles to allocate ", 500, " blocks from Hamstraaja");
                
                start = Genode::Trace::timestamp();
                for (unsigned i = 0; i < 500; i++) {
                    _hamstraaja->free(ptrs[i], 0);
                }
                end = Genode::Trace::timestamp();
                Genode::log("Took ", (end - start), " cycles to free ", 500, " blocks from Hamstraaja");
			}

			Genode::log("Trying to get memory from each NUMA region");
			Tukija::Tip::tip()->for_each([&](Tukija::Tip::Domain &dom) {
				void *ptr = _hamstraaja->aligned_alloc(64, 0, dom.id);
				Genode::log("[node ", dom.id, "] ", ptr);
			});
			
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
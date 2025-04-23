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

namespace Ealan::Memory {
    class CoreheapTest;
    using namespace Genode;
}

class Ealan::Memory::CoreheapTest
{
    private:
        Env &_env;
        Core_heap<64, 2048> _heap{_env.pd(), _env.rm()};

    public:

        CoreheapTest(Env &env) : _env(env)
        {
            log("Starting tests for Core_heap");

            void *p = _heap.aligned_alloc(42, 16);
            log("Allocated memory at ", p);
        }
};

void Component::construct(Genode::Env &env)
{
    static Ealan::Memory::CoreheapTest test(env);
}
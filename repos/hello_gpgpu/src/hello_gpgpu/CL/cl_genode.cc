#include "cl_genode.h"

cl_genode::cl_genode(Genode::Env& env, unsigned long size) : env(env), heap{ env.ram(), env.rm() }, allocator(&heap), mapped_base(0), backend_driver(env)
{
    // get shared memory with driver
    Genode::Ram_dataspace_capability ram_cap;
    backend_driver.register_vm(size, ram_cap);

    // attach it to vm
    mapped_base = env.rm().attach(ram_cap);
    
    // use it in allocator
    allocator.add_range(mapped_base, size);
}

cl_genode::~cl_genode()
{

}

void* cl_genode::aligned_alloc(Genode::uint32_t alignment, Genode::uint32_t size)
{
    if(alignment == 0x1000)
    {
        alignment = 12;
    }
    else if(alignment != 0x0)
    {
        Genode::error("[OCL] Unsupported alignment: ", alignment);
        return nullptr;
    }

    return allocator.alloc_aligned(size, alignment).convert<void *>(

		[&] (void *ptr) { return ptr; },

		[&] (Genode::Range_allocator::Alloc_error) -> void * {
            Genode::error("[OCL] Error in driver allocation!");
            return nullptr; 
        }
    );
}

void* cl_genode::alloc(Genode::uint32_t size)
{
    return allocator.alloc(size);
}

void cl_genode::free(void* addr)
{
    allocator.free(addr);
}

void cl_genode::testRPC()
{
	int i = 42;
	Genode::log("send number ref with RPC: ", i);
	int ret = backend_driver.say_hello(i);
	Genode::log("got number back from RPC: ", ret, "; number ref: ", i);
}

int cl_genode::enqueue_task(struct kernel_config* kconf)
{
    // convert virt vm addr to offset
    for(int i = 0; i < kconf->buffCount; i++)
    {
        kconf->buffConfigs[i].buffer = (void*)((Genode::addr_t)kconf->buffConfigs[i].buffer - mapped_base);
    }
    kconf->buffConfigs = (struct buffer_config*)((Genode::addr_t)kconf->buffConfigs - mapped_base);
    kconf->kernelName = (char*)((Genode::addr_t)kconf->kernelName - mapped_base);
    kconf->binary = (Genode::uint8_t*)((Genode::addr_t)kconf->binary - mapped_base);

    // send RPC
    return backend_driver.start_task((unsigned long)kconf - mapped_base);
}

void cl_genode::wait(struct kernel_config* kconf)
{
    while(!kconf->finished)
    {
        asm("nop");
    }
}

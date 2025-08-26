#include "cl_genode.h"

cl_genode::cl_genode(Genode::Env& env, unsigned long size) : env(env), allocator(), mapped_base(0), backend_driver(env), shm_allocator(), shm_mapped_base{0, }
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

void cl_genode::get_shm(int shmid)
{
    // create shm for gpu
    Genode::size_t total_size = 0;
    Genode::Ram_dataspace_capability ram_cap;
    while (total_size == 0)
    {
        backend_driver.ask_shm(shmid, total_size, ram_cap);
    }

    // attach shm to vm
    shm_mapped_base[shmid] = env.rm().attach(ram_cap);

    // use it in allocator
    shm_allocator[shmid].add_range(shm_mapped_base[shmid], total_size);

}

void* cl_genode::aligned_alloc(Genode::uint32_t alignment, Genode::uint32_t size)
{
    return allocator.alloc_aligned(alignment, size);
}

void* cl_genode::alloc(Genode::uint32_t size)
{
    return allocator.alloc(size);
}

void cl_genode::free(void* addr)
{
    allocator.free(addr);
}

void* cl_genode::shm_aligned_alloc(int shmid, Genode::uint32_t alignment, Genode::uint32_t size)
{
    return shm_allocator[shmid].alloc_aligned(alignment, size);
}

void* cl_genode::shm_alloc(int shmid, Genode::uint32_t size)
{
    return shm_allocator[shmid].alloc(size);
}

void cl_genode::shm_free(int shmid, void* addr)
{
    shm_allocator[shmid].free(addr);
}

void cl_genode::enqueue_task(struct kernel_config* kconf)
{
    // convert virt vm addr to offset
    for(int i = 0; i < kconf->buffCount; i++)
    {
        const Genode::addr_t mbase = kconf->buffConfigs[i].shmid == -1 ? mapped_base : shm_mapped_base[kconf->buffConfigs[i].shmid];
        kconf->buffConfigs[i].buffer = (void*)((Genode::addr_t)kconf->buffConfigs[i].buffer - mbase);
    }
    kconf->buffConfigs = (struct buffer_config*)((Genode::addr_t)kconf->buffConfigs - mapped_base);
    kconf->kernelName = (char*)((Genode::addr_t)kconf->kernelName - mapped_base);
    kconf->binary = (Genode::uint8_t*)((Genode::addr_t)kconf->binary - mapped_base);

    // send RPC
    backend_driver.start_task((unsigned long)kconf - mapped_base);
}

void cl_genode::wait(struct kernel_config* kconf)
{
    while(!kconf->finished)
    {
        asm("nop");
    }
}

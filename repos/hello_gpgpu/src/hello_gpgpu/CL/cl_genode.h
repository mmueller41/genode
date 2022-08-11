#ifndef CL_GENODE_H
#define CL_GENODE_H

// genode
#include <base/env.h>

// allocator
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <dataspace/client.h>

// pci
#include <legacy/x86/platform_session/connection.h>

// rpc
#include <hello_gpgpu_session/connection.h>

// driver
#include <gpgpu/gpgpu.h>

class cl_genode
{
private:
    // genode enviroment
    Genode::Env& env;

    // allocator
    Genode::Heap heap;
    Genode::Allocator_avl allocator;
    Genode::Ram_dataspace_capability ram_cap;
    Genode::addr_t mapped_base;
    Genode::addr_t base;

    // pci
	Platform::Connection pci;

    // rpc
    gpgpu::Connection backend_driver;

    // do not allow copies
    cl_genode(const cl_genode& copy) = delete;
    cl_genode& operator=(const cl_genode& src) = delete;

public:
    /**
     * @brief Construct a new gpgpu genode object
     * 
     * @param e 
     */
    cl_genode(Genode::Env& e, unsigned long size);

    /**
     * @brief Destroy the gpgpu genode object
     * 
     */
    ~cl_genode();

    /**
     * @brief allocate aligned memory
     * 
     * @param alignment the alignment
     * @param size the size in bytes
     * @return void* the address of the allocated memory
     */
    void* aligned_alloc(Genode::uint32_t alignment, Genode::uint32_t size);

    /**
     * @brief 
     * 
     * @param size 
     * @return void* 
     */
    void* alloc(Genode::uint32_t size);

    /**
     * @brief free memory
     * 
     * @param addr the address of the memory to be freed
     */
    void free(void* addr);

    /**
     * @brief converts a virtual address into a physical address
     * 
     * @param virt the virtual address
     * @return addr_t the physical address
     */
    Genode::addr_t virt_to_phys(Genode::addr_t virt) const
    {
		return virt - mapped_base + base;
    }

    /**
     * @brief 
     * 
     */
    void testRPC();

    /**
     * @brief 
     * 
     * @param kconf 
     * @return int 
     */
    int enqueue_task(struct kernel_config* kconf);

    /**
     * @brief 
     * 
     * @param kconf 
     */
    void wait(struct kernel_config* kconf);
};

#endif // CL_GENODE_H

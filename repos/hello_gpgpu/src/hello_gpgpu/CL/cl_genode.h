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
#include <gpgpu_virt/connection.h>

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
    Genode::addr_t mapped_base;

    // rpc
    gpgpu_virt::Connection backend_driver;

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
     * @brief Get the Alloc object
     * 
     * @return Genode::Allocator_avl& 
     */
    Genode::Allocator_avl& getAlloc() {
        return allocator;
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

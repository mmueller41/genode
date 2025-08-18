#include <base/log.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>

#include <libc/component.h>

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// OpenCL
#define CL_TARGET_OPENCL_VERSION 100
#include "../hello_gpgpu/CL/cl.h"

// rpc
#include <gpgpu_virt/connection.h>

// stupid alloc
#include "../hello_gpgpu/allocator_stupid.h"

namespace ns_convolution_2d{int main(int argc, char *argv[]);};

struct consumer_conv
{
    Genode::Env &env;
    gpgpu_virt::Connection backend_driver;
    Genode::Allocator_stupid allocator;

    const unsigned long size = 0x40000000;
    cl_genode clg;
    Genode::Ram_dataspace_capability vgpu_mem_ram_cap;
    Genode::Ram_dataspace_capability vgpu_shm_ram_cap;

    volatile uint8_t *ready;
    const unsigned long img_size = 320 * 240 * sizeof(float);
    volatile float *data;

    void init()
    {
        Genode::log("===Init Consumer Conv===");
		clInitGenode(clg);

        // register vgpu (optional?)
        const unsigned long size_vgpu_mem = 0x1000;
        backend_driver.register_vm(size_vgpu_mem, vgpu_mem_ram_cap);

        // create shm for gpu
        const unsigned long id = 0;
        Genode::size_t total_size = 0;
        while (total_size == 0)
        {
            backend_driver.ask_shm(id, total_size, vgpu_shm_ram_cap);
        }

        // attach shm to vm
        Genode::addr_t mapped_base = env.rm().attach(vgpu_shm_ram_cap);
        clg.add_shm_mapped_base(id, mapped_base);

        // use it in allocator
        allocator.add_range(mapped_base, total_size);

        // alloc whole data
        ready = (uint8_t *)allocator.alloc(1);
        data = (float *)allocator.alloc_aligned(0x10000, img_size);
    }

    void run()
    {
        Genode::log("===Run Consumer Conv===");

        Libc::with_libc([&]
                        {
                            for(;;)
                            {
                                while (*ready != 0x42);
                                
                                ns_convolution_2d::main(2, (char**)data);

                                //Genode::log(data[0]);
                                sleep(3);
                            } });

        Genode::log("===End===");
        Genode::log("Consumer Conv completed");
    }

    consumer_conv(Genode::Env &e) : env(e), backend_driver(env), allocator(), clg(env, size), ready(nullptr), data(nullptr)
    {
    }
};

void Libc::Component::construct(Libc::Env &env)
{
    static consumer_conv p(env);
    p.init();
    p.run();
}

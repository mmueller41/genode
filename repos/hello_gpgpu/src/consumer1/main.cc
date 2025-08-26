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

namespace ns_OpenSurf{int main(int argc, char *argv[]);};

struct consumer_conv
{
    Genode::Env &env;
    const unsigned long size = 0x40000000;
    cl_genode clg;

    volatile uint8_t *ready;
    const unsigned long img_size = 320 * 240 * sizeof(float);
    volatile float *data;

    void init()
    {
        Genode::log("===Init Consumer Surf===");
		clInitGenode(clg);

        // create shm for gpu
        const unsigned long id = 0;
        clg.get_shm(id);

        // alloc whole data
        ready = (uint8_t *)clg.shm_alloc(id, 1);
        data = (float *)clg.shm_aligned_alloc(id, 0x10000, img_size);
    }

    void run()
    {
        Genode::log("===Run Consumer Surf===");

        Libc::with_libc([&]
                        {
                            for(;;)
                            {
                                while (*ready != 0x42);

                                ns_OpenSurf::main(2, (char**)data);

                                //Genode::log(data[0]);
                                sleep(3);
                            } });

        Genode::log("===End===");
        Genode::log("Consumer Surf completed");
    }

    consumer_conv(Genode::Env &e) : env(e), clg(env, size), ready(nullptr), data(nullptr)
    {
    }
};

void Libc::Component::construct(Libc::Env &env)
{
    static consumer_conv p(env);
    p.init();
    p.run();
}

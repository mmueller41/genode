#include <base/log.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <util/misc_math.h>

#include <libc/component.h>

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// rpc
#include <gpgpu_virt/connection.h>

// stupid alloc
#include "../hello_gpgpu/allocator_stupid.h"

// imgs
#include "frac_320_240.h"
#include "frac2_320_240.h"

namespace fake_cv
{
    class Mat
    {
    public:
        int rows;
        int cols;
        int step;
        float *data;
        template <typename T>
        T *ptr(int off)
        {
            return (T *)&data[off * sizeof(T)];
        }
    };

    Mat imread(const char *img)
    {
        Mat m;
        m.rows = height;
        m.cols = width;
        m.step = width * sizeof(float);

        const size_t size = m.rows * m.cols;
        m.data = (float *)malloc(size * sizeof(float));

        for (size_t i = 0; i < size; ++i)
        {
            unsigned int px[3];
            HEADER_PIXEL(img, px);
            const unsigned int g = 0.298936021293775 * px[0] + 0.587043074451121 * px[1] + 0.114020904255103 * px[2];
            m.data[i] = g / 251.; // 255.
        }

        return m;
    }
}

typedef fake_cv::Mat Image;

fake_cv::Mat getGray(const fake_cv::Mat &img)
{
    return img;
}

struct producer
{
    Genode::Env &env;
    gpgpu_virt::Connection backend_driver;
    Genode::Allocator_stupid allocator;

    float *img1;
    float *img2;
    unsigned long img_size;
    volatile uint8_t *ready;
    volatile float *data;

    void init()
    {
        Genode::log("===Init Producer===");

        // register vgpu (optional?)
        const unsigned long size_vgpu_mem = 0x1000;
        Genode::Ram_dataspace_capability vgpu_mem_ram_cap;
        backend_driver.register_vm(size_vgpu_mem, vgpu_mem_ram_cap);

        // create shm for gpu
        const unsigned long size_vgpu_shm = 0x100000;
        Genode::Ram_dataspace_capability vgpu_shm_ram_cap;
        backend_driver.register_shm(size_vgpu_shm, vgpu_shm_ram_cap);

        // attach shm to vm
        Genode::addr_t mapped_base = env.rm().attach(vgpu_shm_ram_cap);

        // use it in allocator
        allocator.add_range(mapped_base, size_vgpu_shm);

        // set not ready
        ready = (uint8_t *)allocator.alloc(1);

        // load img1 and img2
        const Image s1 = fake_cv::imread(header_data);
        Image i1 = getGray(s1);
        img1 = (float *)i1.ptr<float>(0);
        const Image s2 = fake_cv::imread(header_data2);
        Image i2 = getGray(s2);
        img2 = (float *)i2.ptr<float>(0);

        img_size = Genode::max(i1.rows * i2.cols * sizeof(float), i2.rows * i2.cols * sizeof(float));

        // alloc whole data
        data = (float *)allocator.alloc_aligned(0x10000, img_size);
    }

    void run()
    {
        Genode::log("===Run Producer===");

        Libc::with_libc([&]
                        {
            int flip = 0;

            for (;;)
            {
                // fetch new img
                *ready = 0x43;
                memcpy((void*)data, flip ? img1 : img2, img_size);
                *ready = 0x42;
                flip = !flip;
                Genode::log("===New Image ready: ", flip, " ===");
                
                // sleep for 5s
                sleep(5);
            } });

        Genode::log("===End===");
        Genode::log("Producer completed");
    }

    producer(Genode::Env &e) : env(e), backend_driver(env), allocator(), img1(nullptr), img2(nullptr), img_size(0), ready(nullptr), data(nullptr)
    {
    }
};

void Libc::Component::construct(Libc::Env &env)
{
    static producer p(env);
    p.init();
    p.run();
}

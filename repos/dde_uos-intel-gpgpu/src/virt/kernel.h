#ifndef KERNEL_H
#define KERNEL_H

#include <util/fifo.h>

#define GENODE
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include <base/fixed_stdint.h>

namespace gpgpu_virt {

    /**
     * @class This class represents a kernel 
     * 
     */
    class Kernel : public Genode::Fifo<Kernel>::Element
    {
        private:
            struct kernel_config* kconf;

	        Kernel(const Kernel &copy) = delete;

        public:
            Kernel(struct kernel_config* k) : kconf(k) {}

            inline struct kernel_config* get_config() { return kconf; }
    };
}

#endif // KERNEL_H

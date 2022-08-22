#ifndef VGPU_H
#define VGPU_H

#define GENODE
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "../uos-intel-gpgpu/driver/ppgtt32.h"
#include <util/list.h>
#include "kernel.h"

namespace gpgpu {

    class VGpu : public Genode::List<gpgpu::VGpu>::Element
    {
        private:
            PPGTT32* ppgtt;
            Genode::List<Kernel> ready_list;

        public:
            /**
             * @brief Construct a new VGpu object
             */
            VGpu() : ppgtt(nullptr), ready_list() {}

            /**
             * @brief Add a kernel to the vGPU's ready list 
             * 
             * @param kernel - the kernel object to enqueue
             */
            void add_kernel(Kernel* kernel) {
                //kernel->get_config()->gtt = ppgtt;
                ready_list.insert(kernel);
            }

            /**
             * @brief 
             * 
             * @return true 
             * @return false 
             */
            bool has_kernel() const
            {
                return ready_list.first() == nullptr;
            }

            /**
             * @brief Dequeue a kernel from the ready list
             * 
             * @return First kernel image in ready list 
             */
            Kernel* take_kernel() { 
                Kernel* k = ready_list.first();
                ready_list.remove(k);
                return k;
            }
    };
}

#endif // VGPU_H

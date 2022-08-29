#ifndef VGPU_H
#define VGPU_H

#include <util/list.h>
#include "kernel.h"

// driver
#define GENODE
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "../uos-intel-gpgpu/driver/ppgtt32.h"

namespace gpgpu_virt {

    class VGpu : public Genode::List<VGpu>::Element
    {
        private:
            // context of vgpu
            context* ctx;

            /// list of gpgpu tasks for this vpgu
            Genode::List<Kernel> ready_list;

        public:
            /**
             * @brief Construct a new VGpu object
             */
            VGpu() : ctx(nullptr), ready_list() {}

            /**
             * @brief Add a kernel to the vGPU's ready list 
             * 
             * @param kernel - the kernel object to enqueue
             */
            void add_kernel(Kernel* kernel) {
                kernel->get_config()->ctx = ctx; // set context
                ready_list.insert(kernel);
            }

            /**
             * @brief 
             * 
             */
            void allocContext()
            {
                ctx = GPGPU_Driver::getInstance().createContext();
            }

            /**
             * @brief 
             * 
             */
            void freeContext()
            {
                GPGPU_Driver::getInstance().freeContext(ctx);
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

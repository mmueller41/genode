#ifndef VGPU_H
#define VGPU_H

#include <base/log.h>
#include "strategies/config.h"
#include "kernel.h"

#ifdef SCHED_CFS
    #include "strategies/cfs_entry.h"
#else
    #include "strategies/util/wf_queue.h"
#endif // SCHED_CFS

// driver
#define GENODE
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "../uos-intel-gpgpu/driver/ppgtt32.h"

namespace gpgpu_virt {

#ifdef SCHED_CFS
    class VGpu : public cfs_entry
#else
    class VGpu : public util::WFQueue::Chain
#endif // SCHED_CFS
    {
        private:
            // context of vgpu
            context* ctx;

            /// list of gpgpu tasks for this vpgu
            Genode::Fifo<Kernel> ready_list;

            /// priority of vgpu
            int prio;

        public:
            /**
             * @brief Construct a new VGpu object
             */
            VGpu() : ctx(nullptr), ready_list(), prio(-1) {}

            /**
             * @brief Set the Priority
             * 
             * @param p 
             */
            void setPriority(int p)
            {
                prio = p;
            }

            /**
             * @brief Get the Priority
             * 
             * @return int 
             */
            int getPriority()
            {
                return prio;
            }

            /**
             * @brief Add a kernel to the vGPU's ready list 
             * 
             * @param kernel - the kernel object to enqueue
             */
            void add_kernel(Kernel* kernel) {
                kernel->get_config()->ctx = ctx; // set context
                ready_list.enqueue(*kernel);
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
                return ready_list.empty() == false;
            }

            /**
             * @brief Dequeue a kernel from the ready list
             * 
             * @return First kernel image in ready list 
             */
            Kernel* take_kernel() { 
                Kernel* ret = nullptr;
                ready_list.dequeue([&ret](Kernel& k){
                    ret = &k;
                });
                return ret;
            }

            /**
            * @brief Print bench
            */
            void print_vgpu_bench(unsigned long i)
            {
                Genode::log("bench result of vgpu ", i);
                GPGPU_Driver::getInstance().printBenchResults();
            };
    };
}

#endif // VGPU_H

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <util/list.h>
#include "vgpu.h"

namespace gpgpu {

    class Scheduler
    {
        private:
            VGpu* _curr_vgpu;
            Genode::List<VGpu> _run_list;
            bool idle;

        public:

           Scheduler() : _curr_vgpu(nullptr), _run_list(), idle(true) { }

            /**
             * @brief Select next vGPU from run list
             * @details At the moment, round-robin is the only implemented strategy. 
             *          TODO: Implement interface for strategies and strategies             * 
             */
            void schedule_next();

            /**
             * @brief Switch to new vGPU's context
             * 
             * @param vgpu - vGPU to switch to 
             */
            void dispatch(VGpu& vgpu) {
                // TODO: Implement context switch using GPGPU driver 
                // atm no preemption supported => do nothing here
                (void)vgpu;
            }

            /**
             * @brief Implmentation for the handling of events from the GPU
             * @details The handler is especially important for scheduling the next vGPU and for 
             *          executing kernels. It is the target for interrupts coming from the GPGPU driver, e.g. when
             *          a kernel has finished its execution. 
             */
            void handle_gpu_event();

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void add_vgpu(VGpu* vgpu)
            {
                _run_list.insert(vgpu);
            }

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void remove_vgpu(VGpu* vgpu)
            {
                _run_list.remove(vgpu);
            }

            /**
             * @brief 
             * 
             * @return true 
             * @return false 
             */
            bool is_idle() const
            {
                return idle;
            }
    };
}

#endif // SCHEDULER_H

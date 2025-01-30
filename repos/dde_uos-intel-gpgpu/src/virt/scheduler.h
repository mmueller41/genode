#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../config.h"
#include "vgpu.h"
#include "kernel.h"

#ifdef SCHED_CFS
    #include "strategies/util/rbtree.h"
    #include "strategies/cfs.h"
#else
    #include "strategies/util/wf_queue.h"
    #include "strategies/rr.h"
#endif // SCHED_CFS

// genode instance
#include "../gpgpu/gpgpu_genode.h"
extern gpgpu::gpgpu_genode* _global_gpgpu_genode;

// driver
#define GENODE
#include <driver/gpgpu_driver.h>

namespace gpgpu_virt {

    template<typename S> class Scheduler
    {
        private:
            S strat;
            VGpu* _curr_vgpu;
            bool idle;
            
            Scheduler(const Scheduler &copy) = delete;
            Scheduler(Scheduler&&) = delete;
            Scheduler& operator=(const Scheduler&) = delete;
            Scheduler& operator=(Scheduler&&) = delete;

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

        public:
            Scheduler() : strat(), _curr_vgpu(nullptr), idle(true) { }

            /**
             * @brief Implmentation for the handling of events from the GPU
             * @details The handler is especially important for scheduling the next vGPU and for 
             *          executing kernels. It is the target for interrupts coming from the GPGPU driver, e.g. when
             *          a kernel has finished its execution. 
             */
            void handle_gpu_event()
            {
#ifndef QEMU_TEST
                // reduce frequency
                GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
                gpgpudriver.setMinFreq();
#endif // QEMU_TEST

                /* Switch to next vGPU in the run list */
                _curr_vgpu = strat.nextVGPU();

                // If no vGPU to schedule, this means that we don't have any clients with anymore.
                if(_curr_vgpu == nullptr)
                {
                    idle = true;
                    return;
                }

                idle = false;

                Kernel* next = _curr_vgpu->take_kernel();

                // switch context
                dispatch(*_curr_vgpu);

#ifdef QEMU_TEST
                // sim interupt
                handle_gpu_event();

                // set finished and call callback
                next->get_config()->finished = true;
                if(next->get_config()->finish_callback)
                {
                    next->get_config()->finish_callback();
                }
#else
                // set frequency
                gpgpudriver.setMaxFreq();

                // run gpgpu task
                gpgpudriver.enqueueRun(*next->get_config());
#endif // QEMU_TEST

                // free kernel object
                // kernel_config will not be freed, just the Queue object!
                _global_gpgpu_genode->free(next);
            }

            /**
             * @brief 
             * 
             */
            void trigger()
            {
                const bool b = __sync_lock_test_and_set(&idle, false);
                if(b)
                {
                    handle_gpu_event();
                }
            }

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void add_vgpu(VGpu* vgpu)
            {
                strat.addVGPU(vgpu);
            }

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void remove_vgpu(VGpu* vgpu)
            {
                strat.removeVGPU(vgpu);
            }

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void update_vgpu(VGpu* vgpu)
            {
                strat.updateVGPU(vgpu);
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

#ifdef SCHED_CFS
    using GPGPUScheduler = Scheduler<gpgpu_virt::CompletlyFair>;
#else
    using GPGPUScheduler = Scheduler<gpgpu_virt::RoundRobin>;
#endif // SCHED_CFS
}

#endif // SCHEDULER_H

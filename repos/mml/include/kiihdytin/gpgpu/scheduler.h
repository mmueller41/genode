/*
 * \brief  Scheduler interface for the GPGPU, select which vGPU to choose next.
 * \author Michael Müller
 * \date   2022-07-15
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is distributed under the terms of the
 * GNU Affero General Public License version 3.
 */
#pragma once

#include <driver/lib/wf_queue.h>
#include <driver/gpgpu_driver.h>
#include <driver/ppgtt32.h>
#include "vgpu.h"
#include "kernel.h"

namespace Kiihdytin::GPGPU {

    class Scheduler
    {
        private:
            VGpu *_curr_vgpu;
            // GPGPU_Driver _driver; /* TODO: Use driver session */
            WFQueue _run_list;
            /* TODO: Define handler object for GPGPU driver session to receive interrupts. */

        public:

           Scheduler()
           {
                // TODO: Initialize GPU driver
                
                // TODO: Register interrupt/event handler for the GPGPU driver session.
           }

            /**
             * @brief Select next vGPU from run list
             * @details At the moment, round-robin is the only implemented strategy. 
             *          TODO: Implement interface for strategies and strategies             * 
             */
            void schedule_next() {
                VGpu *next;

                if ((next = static_cast<VGpu*>(_run_list.dequeue()))) {
                    this->dispatch(*next);
                    _curr_vgpu = next;
                    _run_list.enqueue(next);
                } else
                    _curr_vgpu = nullptr; 
            }

            /**
             * @brief Switch to new vGPU's context
             * 
             * @param vgpu - vGPU to switch to 
             */
            void dispatch(VGpu &vgpu) {
                // TODO: Implement context switch using GPGPU driver 
            }

            /**
             * @brief Implmentation for the handling of events from the GPU
             * @details The handler is especially important for scheduling the next vGPU and for 
             *          executing kernels. It is the target for interrupts coming from the GPGPU driver, e.g. when
             *          a kernel has finished its execution. 
             */
            void handle_gpu_event() {
                // TODO: Check for error conditions

                // TODO: Handle finish of kernel

                /* Switch to next vGPU in the run list */
                schedule_next();

                /* If no vGPU to schedule, this means that we don't have any clients anymore.
                 * Thus, there are also no kernels anymore to run. */
                if (_curr_vgpu == nullptr) 
                    return;

                Kernel *next = _curr_vgpu->take_kernel();

                if (!next) /* If there is no kernel for the vGPU left */
                    schedule_next(); /* pick the next vGPU, maybe it has got some kernels for us. */
                
                // TODO: execute kernel using GPGPU driver
            }
    };
}
/*
 * \brief  Representation of a "virtual" GPU, used as abstraction for the real thing.
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

#include <driver/gpgpu_driver.h>
#include "kernel.h"
#include <driver/ppgtt32.h>
#include <driver/lib/chain.h>
#include <driver/lib/wf_queue.h>

    namespace Kiihdytin::GPGPU {

    class Context;

    class VGpu : public Chain
    {
        private:
            // Context _context; TODO: implement context images
            PPGTT32 &_ppgtt;
            WFQueue _ready_list;

        public:
            /**
             * @brief Construct a new VGpu object
             * 
             * @param ppgtt - PPGTT mapping phyisical addresses from the client's rm space to gpu addresses 
             */
            VGpu(PPGTT32 &ppgtt) : _ppgtt(ppgtt) {}

            /**
             * @brief Add a kernel to the vGPU's ready list 
             * 
             * @param kernel - the kernel object to enqueue
             */
            void add_kernel(Kernel &kernel) {
                _ready_list.enqueue(&kernel);
            }

            /**
             * @brief Get saved GPU context for this VGPU 
             * 
             * @return GPU context image for this VGPU
             * TODO: implement saving the context of the GPU using the GPGPU driver 
             */
            Context get_context();

            /**
             * @brief Dequeue a kernel from the ready list
             * 
             * @return First kernel image in ready list 
             */
            Kernel *take_kernel() { return static_cast<Kernel*>(_ready_list.dequeue()); }

            /**
             * @brief Get the ppgtt object
             * 
             * @return PPGTT 
             */
            PPGTT32 &get_ppgtt() { return _ppgtt;  }


    };
}
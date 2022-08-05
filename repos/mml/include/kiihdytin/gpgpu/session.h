/*
 * \brief  Interface definition of a GPU session
 * \author Michael Müller
 * \date   2022-07-17
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is distributed under the terms of the 
 * GNU Affero General Public License version 3.
 */
#pragma once

#include <session/session.h>
#include <base/rpc.h>
#include "vgpu.h"

    namespace Kiihdytin::GPGPU {
    class Session;
}

class Kiihdytin::GPGPU::Session : Genode::Session
{
    
    private:
        VGpu &vgpu;

        VGpu& create_vgpu();
        PPGTT32& create_ppgtt();


    public:
	    static const char *service_name() { return "Kiihdytin::GPGPU"; }

	    enum { CAP_QUOTA = 2 }; // TODO: determine actual cap quota

        Session() : vgpu(create_vgpu()) {}

        /* Backend methods */
        virtual void enqueue_kernel(Kernel &kernel) = 0;
        virtual void wait_for_kernel(Kernel &kernel) = 0;
        virtual void abort_kernel(Kernel &kernel) = 0;
        virtual void remove_kernel(Kernel &kernel) = 0;

        /* RPC interface */

        GENODE_RPC(Rpc_enqueue_kernel, void,  enqueue_kernel, Kernel&);
        GENODE_RPC(Rpc_wait_for_kernel, void, wait_for_kernel, Kernel &);
        GENODE_RPC(Rpc_abort_kernel, void, abort_kernel, Kernel &);
        GENODE_RPC(Rpc_remove_kernel, void, remove_kernel, Kernel &);

        GENODE_RPC_INTERFACE(Rpc_enqueue_kernel, Rpc_remove_kernel, Rpc_wait_for_kernel, Rpc_abort_kernel);
};
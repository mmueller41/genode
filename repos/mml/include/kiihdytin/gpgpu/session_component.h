/*
 * \brief  RPC object for a GPGPU session
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

#include <base/component.h>
#include <base/rpc_server.h>
#include "session.h"

namespace Kiihdytin::GPGPU {
    class Session_component;
}

class Kiihdytin::GPGPU::Session_component : public Genode::Rpc_object<Kiihdytin::GPGPU::Session>
{
    public:
        void enqueue_kernel(Kernel &kernel) override;
        void wait_for_kernel(Kernel &kernel) override;
        void abort_kernel(Kernel &kernel) override;
        void remove_kernel(Kernel &kernel) override;

};
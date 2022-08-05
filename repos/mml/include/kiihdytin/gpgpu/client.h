/*
 * \brief  CLient-side interface to a GPGPU session
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

#include <base/rpc_client.h>
#include <base/log.h>
#include "session.h"
#include "kernel.h"

namespace Kiihdytin::GPGPU {
    struct Session_client;
}

struct Kiihdytin::GPGPU::Session_client : Genode::Rpc_client<Kiihdytin::GPGPU::Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) { }

    void enqueue_kernel(Kernel &kernel) override {
        call<Rpc_enqueue_kernel>(kernel);
    }

    void wait_for_kernel(Kernel &kernel) override {
        call<Rpc_wait_for_kernel>(kernel);
    }

    void abort_kernel(Kernel &kernel) override {
        call<Rpc_abort_kernel>(kernel);
    }

    void remove_kernel(Kernel &kernel) override {
        call<Rpc_remove_kernel>(kernel);
    }
};
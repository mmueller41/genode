/*
 * \brief  Suoritin - Task-based CPU Client Interface 
 * \author Michael Müller
 * \date   2023-07-12
 */

/*
 * Copyright (C) 2010-2020 Genode Labs GmbH
 * Copyright (C) 2023 Michael Müller, Osnabrück University
 *
 * This file is part of EalánOS, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#pragma once

#include <suoritin/session.h>
#include <base/rpc_client.h>
#include <base/affinity.h>

struct Tukija::Suoritin::Client : Genode::Rpc_client<Tukija::Suoritin::Session>
{
    explicit Client(Genode::Capability<Tukija::Suoritin::Session> session)
    : Rpc_client<Tukija::Suoritin::Session>(session) { }

    void create_channel(Tukija::Suoritin::Worker const &worker) override 
    {
        call<Rpc_create_channel>(worker);
    }

    void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) override
    {
        call<Rpc_register_worker>(name, cap);
    }

    Genode::Dataspace_capability worker_if() override
    {
        return call<Rpc_suoritin_worker_if>();
    }

    Genode::Dataspace_capability channel_if() override
    {
        return call<Rpc_suoritin_channel_if>();
    }

    Genode::Dataspace_capability event_channel() override
    {
        return call<Rpc_suoritin_event_if>();
    }
};
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

    void create_channel() override 
    {
        call<Rpc_create_channel>();
    }

    void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) override
    {
        call<Rpc_register_worker>(name, cap);
    }

    Capability interface_cap() override {
        return call<Rpc_suoritin_cap>();
    }
};
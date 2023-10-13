/*
 * \brief  Suoritin - Task-based CPU Connection 
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

#include <suoritin/client.h>
#include <base/connection.h>

namespace Tukija {
    namespace Suoritin {
        struct Connection;
    }
}

struct Tukija::Suoritin::Connection : Genode::Connection<Tukija::Suoritin::Session>, Tukija::Suoritin::Client
{
    enum
    {
        RAM_QUOTA = 32UL /* in kilobytes */
    };

    Connection(Genode::Env &env, const char *label="", Genode::Affinity const &affinity = Genode::Affinity())
    : Genode::Connection<Tukija::Suoritin::Session>(env, session(env.parent(), affinity, "ram_quota=%uK, cap_quota=%u, label=\"%s\"", RAM_QUOTA, CAP_QUOTA, label)), Tukija::Suoritin::Client(cap()) {
        Genode::log("Connecting to TASKING service ...");
    }

    void create_channel(Tukija::Suoritin::Worker const &worker) override {
        Tukija::Suoritin::Client::create_channel(worker);
    }

    void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) override {
        Tukija::Suoritin::Client::register_worker(name, cap);
    }

    Genode::Dataspace_capability worker_if() override {
        return Tukija::Suoritin::Client::worker_if();
    }

    Genode::Dataspace_capability channel_if() override {
        return Tukija::Suoritin::Client::channel_if();
    }

    Genode::Dataspace_capability event_channel() override {
        return Tukija::Suoritin::Client::event_channel();
    }
};
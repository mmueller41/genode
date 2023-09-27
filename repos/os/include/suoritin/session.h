/*
 * \brief  Suoritin - Task-based CPU Service 
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

#include <session/session.h>
#include <base/affinity.h>
#include <base/registry.h>
#include <cpu_session/client.h>

namespace Tukija {
    namespace Suoritin {
        struct Session;
        struct Client;
        class Capability;

        struct Channel;
        struct Worker;
        typedef Genode::Registry<Genode::Registered<Channel>> Channel_list;
        typedef Genode::Registry<Genode::Registered<Worker>> Worker_registry;
    }
}

class Tukija::Suoritin::Capability 
{
    private: 
        Genode::Ram_dataspace_capability _worker_space;
        Genode::Ram_dataspace_capability _channel_space;

    public:
        Capability(Genode::Ram_dataspace_capability worker_space, Genode::Ram_dataspace_capability channel_space) :  _worker_space(worker_space), _channel_space(channel_space) {}

        Genode::Ram_dataspace_capability worker_interface() { return _worker_space; }
        Genode::Ram_dataspace_capability channel_space() { return _channel_space; }
};

struct Tukija::Suoritin::Worker : Genode::Interface
{
    Genode::Thread_capability _cap{};
    Genode::Thread::Name _name{};
};

struct Tukija::Suoritin::Channel : Genode::Interface
{
    unsigned long _id{0};
    unsigned long _length{0};
    unsigned long _occupancy{0};
};

struct Tukija::Suoritin::Session : Genode::Session
{
    static const char *service_name() { return "TASKING"; }

    enum
    {
        CAP_QUOTA = 6
    };

    /**
     * @brief List of all channels, i.e. worker queues, of the client cell
     * 
     */
    Channel_list _channels{};

    /**
     * @brief List of worker threads for this client
     * 
     */
    Worker_registry _workers{};

    virtual ~Session() { }
    
    /************************
     ** internal interface **
     ************************/
    Channel_list &channels() { return _channels; }
    Worker_registry &workers() { return _workers; }

    /********************************
     ** Suoritin session interface **
     ********************************/
    virtual void create_channel() = 0;
    virtual void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) = 0;
    virtual Capability interface_cap() = 0;

    GENODE_RPC(Rpc_create_channel, void, create_channel);
    GENODE_RPC(Rpc_register_worker, void, register_worker, Genode::Thread::Name const&, Genode::Thread_capability);
    GENODE_RPC(Rpc_suoritin_cap, Tukija::Suoritin::Capability, interface_cap);

    GENODE_RPC_INTERFACE(Rpc_create_channel, Rpc_register_worker, Rpc_suoritin_cap);
};
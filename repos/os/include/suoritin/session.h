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
        struct Event_channel;
    }
}


struct Tukija::Suoritin::Worker : Genode::Interface
{
    Genode::Thread_capability _cap;
    Genode::Thread::Name _name;
    unsigned long _id;

    inline Genode::Thread_capability cap() { return _cap; }
    inline Genode::Thread::Name name() { return _name; }
    inline unsigned long id() { return _id; }

    Worker(Genode::Thread_capability cap, Genode::Thread::Name &name)
    :
        _cap(cap), _name(name), _id(0) {}
};

struct Tukija::Suoritin::Channel : Genode::Interface
{
    typedef unsigned long Length;
    typedef unsigned long Occupancy;

    unsigned long _id{0};
    alignas(64) volatile unsigned long _length{0};
    alignas(64) volatile unsigned long _occupancy{0};
    alignas(64) unsigned long _worker{0};

    void length(Length increment) { __atomic_add_fetch(&_length, increment, __ATOMIC_RELAXED); }
    void occupancy(Occupancy new_occupancy) { __atomic_store_n(&_occupancy, new_occupancy, __ATOMIC_RELEASE); }
    inline Length length() { return __atomic_load_n(&_length, __ATOMIC_ACQUIRE); }
    inline Occupancy occupancy() { return __atomic_load_n(&_occupancy, __ATOMIC_ACQUIRE); }

    Channel(Worker &worker) : _worker(worker.id()) {
        log("My worker is ", worker.name(), " at ", _worker);
    }
};

struct Tukija::Suoritin::Event_channel : Genode::Interface
{
    enum
    {
        PARENT_REQUEST,
        PARENT_RESPONSE,
        CHILD_REQUEST,
        CHILD_RESPONSE
    };
    alignas(64) volatile bool parent_flag;
    alignas(64) volatile bool child_flag;
    alignas(64) Genode::Parent::Resource_args parent_args;
    alignas(64) Genode::Parent::Resource_args child_args;
};

struct Tukija::Suoritin::Session : Genode::Session
{
    static const char *service_name() { return "TASKING"; }

    enum
    {
        CAP_QUOTA = 6
    };

    virtual ~Session() { }
    
    /********************************
     ** Suoritin session interface **
     ********************************/
    virtual void create_channel(Worker const &worker) = 0;
    virtual void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) = 0;
    virtual Genode::Dataspace_capability worker_if() = 0;
    virtual Genode::Dataspace_capability channel_if() = 0;
    virtual Genode::Dataspace_capability event_channel() = 0;

    GENODE_RPC(Rpc_create_channel, void, create_channel, Worker const&);
    GENODE_RPC(Rpc_register_worker, void, register_worker, Genode::Thread::Name const&, Genode::Thread_capability);
    GENODE_RPC(Rpc_suoritin_worker_if, Genode::Dataspace_capability, worker_if);
    GENODE_RPC(Rpc_suoritin_channel_if, Genode::Dataspace_capability, channel_if);
    GENODE_RPC(Rpc_suoritin_event_if, Genode::Dataspace_capability, event_channel);

    GENODE_RPC_INTERFACE(Rpc_create_channel, Rpc_register_worker, Rpc_suoritin_worker_if, Rpc_suoritin_channel_if, Rpc_suoritin_event_if);
};
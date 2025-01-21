/*
 * \brief  Suoritin - Task-based CPU session component and root component 
 * \author Michael Müller
 * \date   2023-10-10
 */

/*
 * Copyright (C) 2010-2020 Genode Labs GmbH
 * Copyright (C) 2023 Michael Müller, Osnabrück University
 *
 * This file is part of EalánOS, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#pragma once
/* Genode includes */
#include <base/affinity.h>
#include <base/attached_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/session_object.h>
#include <root/component.h>

#include <suoritin/session.h>
#include <suoritin/client.h>
#include <suoritin/connection.h>

namespace Tukija {
    namespace Suoritin {
        class Session_component;
        template <class T> class Allocator;
        class Root_component;
    }
}

template <class T>
class Tukija::Suoritin::Allocator : public Genode::Allocator
{
    using size_t = Genode::size_t;

private:
    size_t _interface_size;
    T* _pos;
    T* _interface;

    Allocator<T>(const Tukija::Suoritin::Allocator<T> &) {}
    Allocator<T> &operator=(const Allocator<T>&) {}

    public : 
    
    Allocator(T *interface, size_t interface_size) : _interface_size(interface_size), _pos(interface), _interface(interface)
    {
    }

        Alloc_result try_alloc(size_t) override
        {
            T *pos = _pos;
            Genode::log("Trying to allocate one interface at ", pos);
            if (pos >= static_cast<T *>(_interface) + _interface_size)
                return Alloc_result(Genode::Ram_allocator::Alloc_error::OUT_OF_RAM);

            _pos++;
            return Alloc_result(static_cast<T *>(pos));
        }

        void free(void *, size_t) override
        { }

        size_t overhead(size_t) const override { return 0; }

        bool need_size_for_free() const override { return false; }

        T *interface() { return _interface; }
};

class Tukija::Suoritin::Session_component : public Genode::Session_object<Tukija::Suoritin::Session>
{
    private:
        Genode::Affinity _affinity;
        Genode::Env &_env;

        Genode::Attached_ram_dataspace _workers_if;
        Genode::Attached_ram_dataspace _channels_if;
        Genode::Attached_ram_dataspace _event_channel;

        Allocator<Worker> _worker_allocator;
        Allocator<Channel> _channel_allocator;


        unsigned long no_channels{0};
        unsigned long no_workers{0};

        template <class T, typename FUNC, typename ...Args>
        void construct(FUNC const &fn, Allocator<T> &alloc, Args ...args) {
            T* object = nullptr;

            try {
                try {
                    object = new (alloc) T(args...);
                    fn(object);
                } catch (Genode::Allocator::Out_of_memory) {
                    Genode::error("Out of RAM on registering worker.");
                    throw;
                }
            } catch (...) {
                if (object)
                    destroy(alloc, object);
                Genode::error("Exception caught registering worker");
                throw;
            }
        }


    public:
        Session_component(  Genode::Rpc_entrypoint &session_ep,
                            Resources const &resources,
                            Label const &label,
                            Diag const &diag,
                            Genode::Env &env,
                            Genode::Affinity &affinity)
        : 
            Genode::Session_object<Session>(session_ep, resources, label, diag),
            _affinity(affinity.space().total() ? affinity : Genode::Affinity(Genode::Affinity::Space(1,1), Genode::Affinity::Location(0,0,1,1))),
            _env(env),
            _workers_if(env.ram(), env.rm(), sizeof(Worker)*affinity.space().total()),
            _channels_if(env.ram(), env.rm(), sizeof(Channel)*affinity.space().total()),
            _event_channel(env.ram(), env.rm(), sizeof(Event_channel)),
            _worker_allocator(_workers_if.local_addr<Worker>(), _affinity.space().total()*sizeof(Worker)),
            _channel_allocator(_channels_if.local_addr<Channel>(), _affinity.space().total()*sizeof(Channel))
        {
        }

        void create_channel(Worker const &worker) override
        {
            try {
                construct<Channel>([&](Channel *) {}, _channel_allocator, worker);
            }
            catch (...)
            {
                Genode::error("Faild to create channel");
            }
            no_channels++;
        }
        void register_worker(Genode::Thread::Name const &name, Genode::Thread_capability cap) override {
            try {
                construct<Worker>([&](Worker *w)
                                  { w->_id = (w - workers_if()); },
                                  _worker_allocator, cap, name);
            }
            catch (...)
            {
                Genode::error("Failed to register worker");
            }
        }

        Genode::Dataspace_capability worker_if() override 
        {
            return _workers_if.cap();
        }

        Genode::Dataspace_capability channel_if() override
        {
            return _channels_if.cap();
        }

        Genode::Dataspace_capability event_channel() override
        {
            return _event_channel.cap();
        }

        /*****
         * Internal interface for use by Hoitaja
         *****/

        inline Worker *workers_if() {
            return _workers_if.local_addr<Worker>();
        }
        inline Channel *channels_if() {
            return _channels_if.local_addr<Channel>();
        }

        inline Worker &worker(unsigned long id) {
            return workers_if()[id];
        }

        void send_request(Genode::Parent::Resource_args &args)
        {
            Event_channel *evtchn = _event_channel.local_addr<Event_channel>();
            evtchn->parent_args = args;
            
            evtchn->parent_flag = true;
        }

        unsigned long channels() { return no_channels; }
};

class Tukija::Suoritin::Root_component 
: public Genode::Root_component<Tukija::Suoritin::Session_component>
{
    private:
        Genode::Registry<Genode::Registered<Session_component>> sessions{};
        Genode::Env &_env;

    public:
        Session_component *_create_session(const char *args) override
        {
            Genode::log("Creating new TASKING session");
             return new(md_alloc()) Genode::Registered<Session_component>(sessions, 
             *this->ep(),
             Genode::session_resources_from_args(args),
             Genode::session_label_from_args(args),
             Genode::session_diag_from_args(args),
             _env, 
             Genode::Affinity(_env.cpu().affinity_space(), Genode::Affinity::Location(0,0,_env.cpu().affinity_space().width(), _env.cpu().affinity_space().height())));
        }

        /* Interal interface for Hoitaja */
        template <typename FN>
        void for_each(FN const &fn) {
            sessions.for_each(fn);
        }

        Root_component(Genode::Env &env, Genode::Allocator &alloc)
        : Genode::Root_component<Session_component>(env.ep(), alloc), _env(env)
        {
            Genode::log("Sta:ted TASKING service");
        }
};
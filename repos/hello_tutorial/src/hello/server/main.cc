/*
 * \brief  Main program of the Hello server
 * \author Björn Döbel
 * \author Norman Feske
 * \date   2008-03-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <hello_session/hello_session.h>
#include <base/rpc_server.h>
#include <timer_session/connection.h>
namespace Hello {
	struct Session_component;
	struct Root_component;
	struct Main;
}


struct Hello::Session_component : Genode::Rpc_object<Session>
{
	unsigned int _id;

	Session_component(unsigned short id) : Genode::Rpc_object<Session>(), _id(id) {}
	
	void say_hello() override {
		Genode::log("I am here... Hello. My id is ", _id, "."); }

	int add(int a, int b) override {
		return a + b; }

	unsigned short id() override {
		return _id;
	}
};


class Hello::Root_component
:
	public Genode::Root_component<Session_component>
{
	protected:
		Timer::Connection &_timer;

		Session_component *_create_session(const char *) override
		{
			Genode::log("creating hello session");
			return new (md_alloc()) Session_component((unsigned short)_timer.elapsed_ms());
		}

	public:

		Root_component(Genode::Entrypoint &ep,
		               Genode::Allocator &alloc,
					   Timer::Connection &timer)
		:
			Genode::Root_component<Session_component>(ep, alloc), _timer(timer)
		{
			Genode::log("creating root component");
		}
};


struct Hello::Main
{
	Genode::Env &env;
	Timer::Connection _timer { env };

	/*
	 * A sliced heap is used for allocating session objects - thereby we
	 * can release objects separately.
	 */
	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Hello::Root_component root { env.ep(), sliced_heap, _timer };

	Main(Genode::Env &env) : env(env)
	{
		/*
		 * Create a RPC object capability for the root interface and
		 * announce the service to our parent.
		 */
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Hello::Main main(env);
}

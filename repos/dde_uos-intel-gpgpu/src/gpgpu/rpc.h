#ifndef RPC_H
#define RPC_H

#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu/session.h>
#include "vgpu.h"

namespace gpgpu {
	struct Session_component;
	struct Root_component;
	struct Main;
}

struct gpgpu::Session_component : Genode::Rpc_object<Session>
{
	VGpu vgpu;
	Genode::addr_t mapped_base;

	Session_component() : vgpu(), mapped_base(0) {}

	int say_hello(int& i) override;

	void register_vm(Genode::Ram_dataspace_capability& ram_cap) override;

	int start_task(unsigned long kconf) override;
};

class gpgpu::Root_component
:
	public Genode::Root_component<Session_component>
{
	protected:
		Session_component *_create_session(const char *) override;

	public:

		Root_component(Genode::Entrypoint &ep,
		               Genode::Allocator &alloc);
};


struct gpgpu::Main
{
	Genode::Env &env;

	/*
	 * A sliced heap is used for allocating session objects - thereby we
	 * can release objects separately.
	 */
	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	gpgpu::Root_component root { env.ep(), sliced_heap };

	Main(Genode::Env &env);
};

#endif // RPC_H

#ifndef RPC_H
#define RPC_H

#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu_virt/session.h>
#include "vgpu.h"

namespace gpgpu_virt
{

struct Session_component : Genode::Rpc_object<Session>
{
	VGpu vgpu;
	Genode::Ram_dataspace_capability ram_cap;
	Genode::addr_t mapped_base;
	Genode::addr_t base;

	Session_component() : vgpu(), ram_cap(), mapped_base(0), base(0) {}

	~Session_component();

	void register_vm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap) override;

	void start_task(unsigned long kconf) override;

	void print_vgpu_bench(unsigned long i) override; 

};

class Root_component
:
	public Genode::Root_component<Session_component>
{
	protected:
		Session_component *_create_session(const char *) override;

	public:

		Root_component(Genode::Entrypoint &ep,
		               Genode::Allocator &alloc);
};


struct Main
{
	Genode::Env &env;

	/*
	 * A sliced heap is used for allocating session objects - thereby we
	 * can release objects separately.
	 */
	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root_component root { env.ep(), sliced_heap };

	Main(Genode::Env &env);
};

}

#endif // RPC_H

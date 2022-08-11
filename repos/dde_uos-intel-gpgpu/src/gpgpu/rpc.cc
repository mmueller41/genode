#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu/session.h>

// genode instance
#include "gpgpu_genode.h"
extern gpgpu_genode* _global_gpgpu_genode;

#define GENODE // use genodes stdint header
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "gpgpu_genode.h"

namespace gpgpu {
	struct Session_component;
	struct Root_component;
	struct Main;
}

struct gpgpu::Session_component : Genode::Rpc_object<Session>
{
	addr_t mapped_base = 0;

	int say_hello(int& i) override
	{
		Genode::log("Hello from uos-intel-gpgpu!");
		Genode::log("Here is your number: ", i);
		i = 64; // change it by ref
		Genode::log("I changed it into ", i);
		return 42;
	}

	void register_vm(Genode::Ram_dataspace_capability& ram_cap) override
	{
		mapped_base = _global_gpgpu_genode->mapMemory(ram_cap);
	}

	int start_task(unsigned long kconf) override
	{
		// convert offset to driver virt addr
		struct kernel_config* kc = (struct kernel_config*)(mapped_base + kconf);
    	kc->binary = (Genode::uint8_t*)((Genode::addr_t)kc->binary + mapped_base);
		// at this point all IO buffers should have phys addrs and all other have driver virt addrs

		// set maximum frequency
		//GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
		//gpgpudriver.setMaxFreq();

		// start gpu task
		//gpgpudriver.enqueueRun(*kc);

		static int id = 0;
		return id++;
	}
};

class gpgpu::Root_component
:
	public Genode::Root_component<Session_component>
{
	protected:
		Session_component *_create_session(const char *) override
		{
			return new (md_alloc()) Session_component();
		}

	public:

		Root_component(Genode::Entrypoint &ep,
		               Genode::Allocator &alloc)
		:
			Genode::Root_component<Session_component>(ep, alloc)
		{

		}
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

	Main(Genode::Env &env) : env(env)
	{
		/*
		 * Create a RPC object capability for the root interface and
		 * announce the service to our parent.
		 */
		env.parent().announce(env.ep().manage(root));
	}
};

void construct_RPC(Genode::Env &env)
{
	static gpgpu::Main main(env);
}

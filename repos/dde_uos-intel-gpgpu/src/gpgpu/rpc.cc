#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu/session.h>
#include "rpc.h"

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

int gpgpu::Session_component::say_hello(int& i)
{
	Genode::log("Hello from uos-intel-gpgpu!");
	Genode::log("Here is your number: ", i);
	i = 64; // change it by ref
	Genode::log("I changed it into ", i);
	return 42;
}

void gpgpu::Session_component::register_vm(Genode::Ram_dataspace_capability& ram_cap)
{
	mapped_base = _global_gpgpu_genode->mapMemory(ram_cap);
}

int gpgpu::Session_component::start_task(unsigned long kconf)
{
	// convert offset to driver virt addr
	struct kernel_config* kc = (struct kernel_config*)(mapped_base + kconf);
	kc->buffConfigs = (struct buffer_config*)((Genode::addr_t)kc->buffConfigs + mapped_base);
	kc->kernelName = (char*)((Genode::addr_t)kc->kernelName + mapped_base);
	kc->binary = (Genode::uint8_t*)((Genode::addr_t)kc->binary + mapped_base);
	// at this point all IO buffers should have phys addrs and all others have driver virt addrs

	// set maximum frequency
	GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
	gpgpudriver.setMaxFreq();

	// start gpu task
	gpgpudriver.enqueueRun(*kc);

	/*
	Kernel* kernel = (Kernel*)_global_gpgpu_genode->aligned_alloc(0, sizeof(Kernel));
	vgpu.add_kernel(kernel);

	free this somewhere
	*/

	static int id = 0;
	return id++;
}

gpgpu::Session_component* gpgpu::Root_component::_create_session(const char *)
{
	return new (md_alloc()) gpgpu::Session_component();
}

gpgpu::Root_component::Root_component(Genode::Entrypoint &ep,
				Genode::Allocator &alloc)
:
	Genode::Root_component<gpgpu::Session_component>(ep, alloc)
{

}

gpgpu::Main::Main(Genode::Env &env) : env(env)
{
	/*
		* Create a RPC object capability for the root interface and
		* announce the service to our parent.
		*/
	env.parent().announce(env.ep().manage(root));
}

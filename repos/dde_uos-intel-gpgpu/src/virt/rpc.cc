#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu_virt/session.h>

#include "rpc.h"
#include "scheduler.h"

// genode instance
#include "../gpgpu/gpgpu_genode.h"
extern gpgpu::gpgpu_genode* _global_gpgpu_genode;
extern gpgpu_virt::Scheduler* _global_sched;

// driver
#define GENODE // use genodes stdint header
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"

namespace gpgpu_virt {

int Session_component::say_hello(int& i)
{
	Genode::log("Hello from uos-intel-gpgpu!");
	Genode::log("Here is your number: ", i);
	i = 64; // change it by ref
	Genode::log("I changed it into ", i);
	return 42;
}

void Session_component::print_vgpu_bench(unsigned long i)
{
	vgpu.print_vgpu_bench(i);
};

void Session_component::register_vm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap_vm)
{
	// create shared mem
	ram_cap = _global_gpgpu_genode->allocRamCap(size, mapped_base, base);
	ram_cap_vm = ram_cap;

	// create vgpu context and add it to scheduler
	vgpu.allocContext();
	_global_sched->add_vgpu(&vgpu);
}

int Session_component::start_task(unsigned long kconf)
{
	// convert offset to driver virt addr
	struct kernel_config* kc = (struct kernel_config*)(kconf + mapped_base);
	kc->buffConfigs = (struct buffer_config*)((Genode::addr_t)kc->buffConfigs + mapped_base);
	for(int i = 0; i < kc->buffCount; i++)
    {
        if(kc->buffConfigs[i].non_pointer_type) // for non pointer set virt addr
        {
			kc->buffConfigs[i].buffer = (void*)((Genode::addr_t)kc->buffConfigs[i].buffer + mapped_base);
        }
		else // for pointer set phys addr
		{
			kc->buffConfigs[i].buffer = (void*)((Genode::addr_t)kc->buffConfigs[i].buffer + base);
		}
    }
	kc->kernelName = (char*)((Genode::addr_t)kc->kernelName + mapped_base);
	kc->binary = (Genode::uint8_t*)((Genode::addr_t)kc->binary + mapped_base);

	// add kernel
	Kernel* kernel = new(_global_gpgpu_genode->getAlloc()) Kernel(kc);
	vgpu.add_kernel(kernel);

	// trigger sched if its idle
	if(_global_sched->is_idle())
	{
		_global_sched->handle_gpu_event();
	}

	static int id = 0;
	/*Genode::log("Kernel ", id);
	for(int i = 0; i < 3; i++)
	{
		Genode::log("\t\trange: ", (int)kc->range[i]);
		Genode::log("\t\twgs: ", (int)kc->workgroupsize[i]);
	}
	for(int i = 0; i < kc->buffCount; i++)
	{
		Genode::log("\tBuffer ", i);
		if(kc->buffConfigs[i].non_pointer_type)
		{
			Genode::log("\t\tvaddr: ", (void*)kc->buffConfigs[i].buffer);
			Genode::log("\t\tval: ", *((uint32_t*)(kc->buffConfigs[i].buffer)));
			Genode::log("\t\tgpuaddr: ", (void*)((addr_t)kc->buffConfigs[i].ga)); // to print this, temporary make the var public
			Genode::log("\t\tpos: ", (uint32_t)kc->buffConfigs[i].pos); // to print this, temporary make the var public
		}
		else
		{
			Genode::log("\t\tvaddr: ", (void*)((Genode::addr_t)kc->buffConfigs[i].buffer - base + mapped_base));
			Genode::log("\t\tpaddr: ", (void*)kc->buffConfigs[i].buffer);
			Genode::log("\t\tgpuaddr: ", (void*)((addr_t)kc->buffConfigs[i].ga));  // to print this, temporary make the var public
			Genode::log("\t\tpos: ", (uint32_t)kc->buffConfigs[i].pos);  // to print this, temporary make the var public
		}
		Genode::log("\t\tsize: ", (int)kc->buffConfigs[i].buffer_size);
	}*/
	return id++;
}

Session_component::~Session_component()
{
	_global_sched->remove_vgpu(&vgpu);
	vgpu.freeContext();
	_global_gpgpu_genode->freeRamCap(ram_cap);
}



Session_component* Root_component::_create_session(const char *)
{
	return new (md_alloc()) Session_component();
}




Root_component::Root_component(Genode::Entrypoint &ep,
				Genode::Allocator &alloc)
:
	Genode::Root_component<Session_component>(ep, alloc)
{

}

Main::Main(Genode::Env &env) : env(env)
{
	/*
		* Create a RPC object capability for the root interface and
		* announce the service to our parent.
		*/
	env.parent().announce(env.ep().manage(root));
}

}

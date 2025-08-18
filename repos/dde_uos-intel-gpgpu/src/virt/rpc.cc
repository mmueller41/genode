#include <base/log.h>
#include <base/heap.h>
#include <root/component.h>
#include <base/rpc_server.h>
#include <gpgpu_virt/session.h>

#include "rpc.h"

// genode instance
#include "../gpgpu/gpgpu_genode.h"
extern gpgpu::gpgpu_genode* _global_gpgpu_genode;
#include "scheduler.h"
extern gpgpu_virt::GPGPUScheduler* _global_sched;

// driver
#define GENODE // use genodes stdint header
#include <driver/gpgpu_driver.h>

namespace gpgpu_virt {

void Session_component::register_vm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap_vm)
{
	// create shared mem
	ram_cap = _global_gpgpu_genode->allocRamCap(size, mapped_base, base);
	ram_cap_vm = ram_cap;

	// create vgpu context and add it to scheduler
#ifndef QEMU_TEST
	vgpu.allocContext();
#endif // QEMU_TEST
	_global_sched->add_vgpu(&vgpu);
}

int SHM_manager::alloc_shm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap_vm)
{
	// get shared memory id
	const int s = __atomic_fetch_add(&shid, 1, __ATOMIC_SEQ_CST);

	if(s >= MAX_SHM_REGIONS)
		return -1;

	// alloc shared memory
	Genode::addr_t mapped_base;
	ram_cap[s] = _global_gpgpu_genode->allocRamCap(size, mapped_base, base[s]);
	sizes[s] = size;
	ram_cap_vm = ram_cap[s];

	return s;
}

void SHM_manager::free_shm(int id)
{
	_global_gpgpu_genode->freeRamCap(ram_cap[id]);
}

void Session_component::register_shm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap_vm)
{
	// create shared mem
	int shid = SHM_manager::getInstance().alloc_shm(size, ram_cap_vm);
	vgpu.assignSHM(shid);
}

void Session_component::ask_shm(int id, Genode::size_t &size, Genode::Ram_dataspace_capability& ram_cap_vm)
{
	// get size
	size = SHM_manager::getInstance().getSize(id);
	if(size == 0) // invalid
		return;

	// get ram cap
	ram_cap_vm = SHM_manager::getInstance().getCap(id);

	// assign id to vgpu
	vgpu.assignSHM(id);
}

void Session_component::start_task(unsigned long kconf)
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
			const Genode::addr_t addrBase = kc->buffConfigs[i].shmid == -1 ? base : SHM_manager::getInstance().getBase(kc->buffConfigs[i].shmid);
			kc->buffConfigs[i].buffer = (void*)((Genode::addr_t)kc->buffConfigs[i].buffer + addrBase);
		}
    }
	kc->kernelName = (char*)((Genode::addr_t)kc->kernelName + mapped_base);
	kc->binary = (Genode::uint8_t*)((Genode::addr_t)kc->binary + mapped_base);

#ifdef VERBOSE
	static int id = 0;
	Genode::log("Kernel ", id);
	Genode::String<32> kname((char const *)kc->kernelName);
	Genode::log("\t\tname: ", kname);
	for(int i = 0; i < 3; i++)
	{
		Genode::log("\t\trange: ", (int)kc->range[i]);
		Genode::log("\t\twgs: ", (int)kc->workgroupsize[i]);
	}
	for(int i = 0; i < kc->buffCount; i++)
	{
		Genode::log("\tBuffer ", i);
		Genode::log("\t\tshmid: ", (int)kc->buffConfigs[i].shmid);
		if(kc->buffConfigs[i].non_pointer_type)
		{
			Genode::log("\t\tvaddr: ", (void*)kc->buffConfigs[i].buffer);
			Genode::log("\t\tval: ", *((uint32_t*)(kc->buffConfigs[i].buffer)));
			//Genode::log("\t\tgpuaddr: ", (void*)((addr_t)kc->buffConfigs[i].ga)); // to print this, temporary make the var public
			//Genode::log("\t\tpos: ", (uint32_t)kc->buffConfigs[i].pos); // to print this, temporary make the var public
		}
		else
		{
			const Genode::addr_t addrBase = kc->buffConfigs[i].shmid == -1 ? base : SHM_manager::getInstance().getBase(kc->buffConfigs[i].shmid);
			Genode::log("\t\tvaddr: ", (void*)((Genode::addr_t)kc->buffConfigs[i].buffer - addrBase));
			Genode::log("\t\tpaddr: ", (void*)kc->buffConfigs[i].buffer);
			//Genode::log("\t\tgpuaddr: ", (void*)((addr_t)kc->buffConfigs[i].ga));  // to print this, temporary make the var public
			//Genode::log("\t\tpos: ", (uint32_t)kc->buffConfigs[i].pos);  // to print this, temporary make the var public
		}
		Genode::log("\t\tsize: ", (int)kc->buffConfigs[i].buffer_size);
	}
	id++;
#endif // VERBOSE

	// add kernel
	Kernel* kernel = new(_global_gpgpu_genode->getAlloc()) Kernel(kc);
	const bool needsUpdate = vgpu.has_kernel() == false;
	vgpu.add_kernel(kernel);

	// trigger vgpu update
	if(needsUpdate)
	{
		_global_sched->update_vgpu(&vgpu);
	}

	// trigger sched
	_global_sched->trigger();
}

Session_component::~Session_component()
{
	_global_sched->remove_vgpu(&vgpu);
#ifndef QEMU_TEST
	vgpu.freeContext();
#endif // QEMU_TEST
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

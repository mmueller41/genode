#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <hello_gpgpu_session/connection.h>
#include "test.h"
#include <libc/component.h>
#include "CL/cl.h"

extern int main(int argc, char *argv[]);

void testvm_construct(Genode::Env &env)
{
	gpgpu::Connection gpgpu(env);

	// allocator
    Genode::Heap heap(env.ram(), env.rm());
    Genode::Allocator_avl alloc(&heap);
	const unsigned int size = 0x10000 * 0x1000;
    Genode::Ram_dataspace_capability ram_cap = env.ram().alloc(size);
    Genode::addr_t mapped_base = env.rm().attach(ram_cap);
    //Genode::addr_t base = Genode::Dataspace_client(ram_cap).phys_addr();
    alloc.add_range(mapped_base, size);

	// test RPC
	gpgpu.say_hello();

	// run the test and hope the best
	//run_gpgpu_test(alloc);

	// run 2mm
	Libc::with_libc([&] {
    	clInitGenode(alloc);
		main(0, 0);
	});

	Genode::log("hello gpgpu completed");
}

void Libc::Component::construct(Libc::Env &env)
{
	testvm_construct(env);
}

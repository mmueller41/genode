#include <base/log.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <dataspace/client.h>
#include <legacy/x86/platform_session/connection.h>

#include <hello_gpgpu_session/connection.h>

#include <libc/component.h>

#define CL_TARGET_OPENCL_VERSION 100
#include "CL/cl.h"
#include "test.h"

extern int main(int argc, char *argv[]);

Genode::Allocator_avl* g_alloc;
void testvm_construct(Genode::Env &env)
{
	Genode::log("===Init VM===");
	// allocator
    Genode::Heap heap(env.ram(), env.rm());
    Genode::Allocator_avl alloc(&heap);
	g_alloc = &alloc;
	const unsigned int size = 0x10000 * 0x1000;
    size_t donate = size;
	Platform::Connection pci(env);
    Genode::Ram_dataspace_capability ram_cap =
        Genode::retry<Genode::Out_of_ram>(
            [&] () {
                return Genode::retry<Genode::Out_of_caps>(
                    [&] () { return pci.alloc_dma_buffer(size, Genode::UNCACHED); },
                    [&] () { pci.upgrade_caps(2); });
            },
            [&] () {
                pci.upgrade_ram(donate);
                donate = donate * 2 > size ? 4096 : donate * 2;
            });
    Genode::addr_t mapped_base = env.rm().attach(ram_cap);
    //Genode::addr_t base = pci.dma_addr(ram_cap);
    alloc.add_range(mapped_base, size);

	// test RPC
	Genode::log("===Test RPC===");
	gpgpu::Connection gpgpu(env);
	int i = 42;
	Genode::log("send number ref with RPC: ", i);
	int ret = gpgpu.say_hello(i);
	Genode::log("got number back from RPC: ", ret, "; number ref: ", i);
	gpgpu.register_vm(ram_cap);
	int* test = (int*)alloc.alloc(sizeof(int));
	*test = 0x42;
	gpgpu.start_task((unsigned long)test - mapped_base);

	// run the test and hope the best
	//run_gpgpu_test(alloc);

	// run 2mm
	Genode::log("===Run 2mm===");
	Libc::with_libc([&] {
		clInitGenode(alloc);
		main(0, 0);
	});

	Genode::log("===End===");
	Genode::log("hello gpgpu completed");
}

void Libc::Component::construct(Libc::Env &env)
{
	testvm_construct(env);
}

#include <base/log.h>
#include <libc/component.h>

#include <unistd.h>

#define CL_TARGET_OPENCL_VERSION 100
#include "CL/cl.h"
#include "test.h"

// bench includes 
#include "benchmark/benchmark_extern.h"


void testvm_construct(Genode::Env &env)
{
	// wait for gpgpu construction
	Libc::with_libc([&] {
		usleep(5000000);
	});

	// init CL env
	Genode::log("===Init VM===");
	const unsigned long size = 0x10000 * 0x1000;
	cl_genode clg(env, size);
	clInitGenode(clg);

	// test RPCs
	Genode::log("===Test RPC===");
	clg.testRPC();
	
	// run the test and hope the best
	Genode::log("===Test GPU===");
	Genode::Heap heap(env.ram(), env.rm());
	Genode::Allocator_avl alloc(&heap);
 	//const unsigned int size = 0x10000 * 0x1000;
	Genode::Ram_dataspace_capability ram_cap = env.ram().alloc(size);
	Genode::addr_t mapped_base = env.rm().attach(ram_cap);
	alloc.add_range(mapped_base, size);
	run_gpgpu_test(alloc);

	// run 2mm
	Genode::log("===Run 2mm===");
	Libc::with_libc([&] {
		ns_2mm::main(0,0);
		ns_3mm::main(0, 0);
		ns_atax::main(0,0);
		ns_bicg::main(0,0);
		ns_doitgen::main(0,0);
		ns_gemm::main(0,0);
		ns_gemver::main(0,0);
		ns_gesummv::main(0,0);
		ns_mvt::main(0,0);
		ns_syr2k::main(0,0);
		ns_syrk::main(0,0);
	});

	Genode::log("===End===");
	Genode::log("hello gpgpu completed");
}

void Libc::Component::construct(Libc::Env& env)
{
	testvm_construct(env);
}

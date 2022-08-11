#include <base/log.h>
#include <libc/component.h>

#define CL_TARGET_OPENCL_VERSION 100
#include "CL/cl.h"
//#include "test.h"

extern int main(int argc, char *argv[]);

void testvm_construct(Genode::Env& env)
{
	Genode::log("===Init VM===");

	// init CL env
	const unsigned long size = 0x10000 * 0x1000;
	cl_genode clg(env, size);
	clInitGenode(clg);

	// test RPCs
	clg.testRPC();
	
	// run the test and hope the best
	//run_gpgpu_test(alloc);

	// run 2mm
	Genode::log("===Run 2mm===");
	Libc::with_libc([&] {
		main(0, 0);
	});

	Genode::log("===End===");
	Genode::log("hello gpgpu completed");
}

void Libc::Component::construct(Libc::Env& env)
{
	testvm_construct(env);
}

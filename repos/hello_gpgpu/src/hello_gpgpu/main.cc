#include <base/log.h>
#include <libc/component.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>

#include <unistd.h>

#define CL_TARGET_OPENCL_VERSION 100
#include "CL/cl.h"
#include "test.h"

// bench includes 
#include "benchmark/benchmark_extern.h"

struct hello_gpgpu
{
	Genode::Env& env;
	unsigned int benchConfig;

	Genode::Attached_rom_dataspace config{env, "config"};

	void handle_config()
	{
		Genode::log("Reading benchmark config...");
		config.update();

		if (!config.valid()) {
			Genode::log("Config is invalid!");
			return;
		}

		benchConfig = config.xml().attribute_value("bench", (unsigned int)0); // => no benchmark
		Genode::log("benchConfig: ", benchConfig);
	}

	Genode::Signal_handler<hello_gpgpu> config_handler {
		env.ep(), *this, &hello_gpgpu::handle_config
	};

	void run()
	{
		// init CL env
		Genode::log("===Init VM===");
		const unsigned long size = 0x40000000;
		cl_genode clg(env, size);
		clInitGenode(clg);

		// run the test and hope the best
		Genode::log("===Test GPU===");
		{
			Genode::Heap heap(env.ram(), env.rm());
			Genode::Allocator_avl alloc(&heap);
			const unsigned long size = 0x10000;
			Genode::Ram_dataspace_capability ram_cap = env.ram().alloc(size);
			Genode::addr_t mapped_base = env.rm().attach(ram_cap);
			alloc.add_range(mapped_base, size);
			run_gpgpu_test(alloc);
			env.ram().free(ram_cap);
		}

		// run selected benchmarks
		Genode::log("===Run PolyBench===");
		Libc::with_libc([&] {
			if(benchConfig & (0x1 << 0))
			{
				Genode::log("===Run 2mm===");
				ns_2mm::main(0, 0);
			}
			if(benchConfig & (0x1 << 1))
			{
				Genode::log("===Run 3mm===");
				ns_3mm::main(0, 0);
			}
			if(benchConfig & (0x1 << 2))
			{
				Genode::log("===Run atax===");
				ns_atax::main(0, 0);
			}
			if(benchConfig & (0x1 << 3))
			{
				Genode::log("===Run bicg===");
				ns_bicg::main(0, 0);
			}
			if(benchConfig & (0x1 << 4))
			{
				Genode::log("===Run doitgen===");
				ns_doitgen::main(0, 0);
			}
			if(benchConfig & (0x1 << 5))
			{
				Genode::log("===Run gemm===");
				ns_gemm::main(0, 0);
			}
			if(benchConfig & (0x1 << 6))
			{
				Genode::log("===Run gemver===");
				ns_gemver::main(0, 0);
			}
			if(benchConfig & (0x1 << 7))
			{
				Genode::log("===Run gesummv===");
				ns_gesummv::main(0, 0);
			}
			if(benchConfig & (0x1 << 8))
			{
				Genode::log("===Run mvt===");
				ns_mvt::main(0, 0);
			}
			if(benchConfig & (0x1 << 9))
			{
				Genode::log("===Run syr2k===");
				ns_syr2k::main(0, 0);
			}
			if(benchConfig & (0x1 << 10))
			{
				Genode::log("===Run syrk===");
				ns_syrk::main(0, 0);
			}

			if(benchConfig & (0x1 << 11))
			{
				Genode::log("===Run gramschmidt===");
				ns_gramschmidt::main(0, 0); // this one is broken (div by 0 and unsolvable input instance) (official Linux version is broken too!)
			}
			if(benchConfig & (0x1 << 12))
			{
				Genode::log("===Run lu===");
				ns_lu::main(0, 0);
			}

			if(benchConfig & (0x1 << 13))
			{
				Genode::log("===Run correlation===");
				ns_correlation::main(0, 0);
			}
			if(benchConfig & (0x1 << 14))
			{
				Genode::log("===Run covariance===");
				ns_covariance::main(0, 0);
			}

			if(benchConfig & (0x1 << 15))
			{
				Genode::log("===Run adi===");
				ns_adi::main(0, 0);
			}
			if(benchConfig & (0x1 << 16))
			{
				Genode::log("===Run convolution_2d===");
				ns_convolution_2d::main(0, 0);
			}
			if(benchConfig & (0x1 << 17))
			{
				Genode::log("===Run convolution_3d===");
				ns_convolution_3d::main(0, 0);
			}
			if(benchConfig & (0x1 << 18))
			{
				Genode::log("===Run fdtd_2d===");
				ns_fdtd_2d::main(0, 0);
			}
			if(benchConfig & (0x1 << 19))
			{
				Genode::log("===Run jacobi_1d_imper===");
				ns_jacobi_1d_imper::main(0, 0);
			}
			if(benchConfig & (0x1 << 20))
			{
				Genode::log("===Run jacobi_2d_imper===");
				ns_jacobi_2d_imper::main(0, 0);
			}

		});

		Genode::log("===End===");
		Genode::log("hello gpgpu completed");
	}

	hello_gpgpu(Genode::Env &e) : env(e), benchConfig(0)
	{
		config.sigh(config_handler);
		handle_config();
	}
};

void Libc::Component::construct(Libc::Env& env)
{
	static hello_gpgpu hg(env);
	hg.run();
}

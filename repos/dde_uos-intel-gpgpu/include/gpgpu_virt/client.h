#ifndef HELLO_GPGPU_CLIENT_H
#define HELLO_GPGPU_CLIENT_H

#include <gpgpu_virt/session.h>
#include <base/rpc_client.h>
#include <base/log.h>

namespace gpgpu_virt
{

struct Session_client : Genode::Rpc_client<Session>
{
	Session_client(Genode::Capability<Session> cap)
	: Genode::Rpc_client<Session>(cap) { }

	void start_task(unsigned long kconf) override
	{
		call<Rpc_start_task>(kconf);
	}

	void register_vm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap) override
	{
		call<Rpc_register_vm>(size, ram_cap);
	}

	void print_vgpu_bench(unsigned long i) override
	{
		call<Rpc_print_vgpu_bench>(i);
	}
};

}

#endif // HELLO_GPGPU_CLIENT_H

#ifndef HELLO_GPGPU_CLIENT_H
#define HELLO_GPGPU_CLIENT_H

#include <gpgpu/session.h>
#include <base/rpc_client.h>
#include <base/log.h>

namespace gpgpu { struct Session_client; }

struct gpgpu::Session_client : Genode::Rpc_client<gpgpu::Session>
{
	Session_client(Genode::Capability<gpgpu::Session> cap)
	: Genode::Rpc_client<gpgpu::Session>(cap) { }

	int start_task(unsigned long kconf) override
	{
		return call<Rpc_start_task>(kconf);
	}

	void register_vm(Genode::Ram_dataspace_capability& ram_cap) override
	{
		call<Rpc_register_vm>(ram_cap);
	}

	int say_hello(int& i) override
	{
		return call<Rpc_say_hello>(i);
	}
};

#endif // HELLO_GPGPU_CLIENT_H

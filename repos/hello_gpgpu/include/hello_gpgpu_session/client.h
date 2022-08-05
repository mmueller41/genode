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

	void say_hello() override
	{
		call<Rpc_say_hello>();
	}
};

#endif // HELLO_GPGPU_CLIENT_H

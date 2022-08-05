#ifndef HELLO_GPGPU_CONNECTION_H
#define HELLO_GPGPU_CONNECTION_H

#include <hello_gpgpu_session/client.h>
#include <base/connection.h>

namespace gpgpu { struct Connection; }

struct gpgpu::Connection : Genode::Connection<gpgpu::Session>, Session_client
{
	Connection(Genode::Env &env)
	:
		/* create session */
		Genode::Connection<gpgpu::Session>(env, session(env.parent(),
		                                                "ram_quota=6K, cap_quota=4")),

		/* initialize RPC interface */
		Session_client(cap()) { }
};

#endif // HELLO_GPGPU_CONNECTION_H

#ifndef HELLO_GPGPU_CONNECTION_H
#define HELLO_GPGPU_CONNECTION_H

#include <gpgpu_virt/client.h>
#include <base/connection.h>

namespace gpgpu_virt
{

struct Connection : Genode::Connection<Session>, Session_client
{
	Connection(Genode::Env &env)
	:
		/* create session */
		Genode::Connection<Session>(env, session(env.parent(),
		                                                "ram_quota=6K, cap_quota=4")),

		/* initialize RPC interface */
		Session_client(cap()) { }
};

}

#endif // HELLO_GPGPU_CONNECTION_H

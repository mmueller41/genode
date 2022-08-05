#ifndef GPGPU_SESSION
#define GPGPU_SESSION

#include <session/session.h>
#include <base/rpc.h>

namespace gpgpu { struct Session; }

struct gpgpu::Session : Genode::Session
{
	static const char *service_name() { return "gpgpu"; }

	enum { CAP_QUOTA = 1 };

	virtual void say_hello() = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_say_hello, void, say_hello);

	GENODE_RPC_INTERFACE(Rpc_say_hello);
};

#endif // GPGPU_SESSION

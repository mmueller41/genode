#ifndef GPGPU_SESSION
#define GPGPU_SESSION

#include <session/session.h>
#include <base/rpc.h>

namespace gpgpu_virt { 

struct Session : Genode::Session
{
	static const char *service_name() { return "gpgpu"; }

	enum { CAP_QUOTA = 1 };

	virtual int say_hello(int& i) = 0;
	virtual void register_vm(Genode::size_t size, Genode::Ram_dataspace_capability& ram_cap) = 0;
	virtual int start_task(unsigned long kconf) = 0;

	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_say_hello, int, say_hello, int&);
	GENODE_RPC(Rpc_register_vm, void, register_vm, Genode::size_t, Genode::Ram_dataspace_capability&);
	GENODE_RPC(Rpc_start_task, int, start_task, unsigned long);

	GENODE_RPC_INTERFACE(Rpc_say_hello, Rpc_register_vm, Rpc_start_task);
};

}

#endif // GPGPU_SESSION

#ifndef __EALANOS__INCLUDE__SHELL__CLIENT_H_
#define __EALANOS__INCLUDE__SHELL__CLIENT_H_

#include "base/capability.h"
#include "base/ram_allocator.h"
#include "region_map/region_map.h"
#include <base/rpc_client.h>
#include <ealanos/shell/session.h>

namespace Ealan
{
	namespace Shell
	{
		using Capability = Genode::Capability<Ealan::Shell::Session>;
		
		struct Client : Genode::Rpc_client<Ealan::Shell::Session> {
			explicit Client(Capability cap) : Genode::Rpc_client<Ealan::Shell::Session>(cap) { }

			Genode::Ram_dataspace_capability connect() override { return call<Rpc_connect>(); }

			void disconnect() override { call<Rpc_disconnect>(); }
			void commit() override { call<Rpc_commit>(); }
		};
	} // namespace Shell
}

#endif
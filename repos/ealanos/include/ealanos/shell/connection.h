#ifndef __EALANOS__INCLUDE__SHELL__CONNECTION_H_
#define __EALANOS__INCLUDE__SHELL__CONNECTION_H_

#include "base/affinity.h"
#include "base/capability.h"
#include "base/ram_allocator.h"
#include "region_map/region_map.h"
#include <base/connection.h>
#include <base/env.h>

#include <ealanos/shell/client.h>

namespace Ealan
{
	namespace Shell
	{
		struct Connection : Genode::Connection<Session>, Client {
				Connection(Genode::Env &env, Genode::Affinity affinity = Genode::Affinity(Genode::Affinity::Space(1,1), Genode::Affinity::Location(0,0)),
				           Label const &label = Label())
					: Genode::Connection<Session>(env, label, Genode::Ram_quota{RAM_QUOTA},
				                                  affinity, Args("")), Client(cap())
				{
				}

				Genode::Ram_dataspace_capability connect() override { return Client::connect(); }

				void disconnect() override { Client::disconnect(); }
				void commit() override { Client::commit(); }
		
		};
	} // namespace Shell
} // namespace Ealan
#endif

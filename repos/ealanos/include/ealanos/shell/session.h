#ifndef __EALANOS__INCLUDE__SHELL__SESSION_H_
#define __EALANOS__INCLUDE__SHELL__SESSION_H_

#include "base/capability.h"
#include "base/ram_allocator.h"
#include "base/rpc.h"
#include "dataspace/capability.h"
#include "region_map/region_map.h"
#include <base/rpc_args.h>
#include <session/session.h>
#include <base/env.h>

namespace Ealan
{
	namespace Shell
	{
		struct Session : Genode::Session 
        {
			static const char *service_name() { return "Shell"; }

			enum { CAP_QUOTA = 2, RAM_QUOTA = 1024 };

			/**
			 * @brief Connect to a shell session by supplying a region map
			 * 
			 * @param rm - region map used for mapping the habitat's config to the client's virtual address space
			 * @return true - Mapping was successful
			 * @return false - An invalid region map was supplied or the mapping failed (e.g. lack of quota or caps)
			 */
			virtual Genode::Ram_dataspace_capability connect() = 0;

            /**
             * @brief Disconnects a client's shell session 
             * 
             */
			virtual void disconnect() = 0;

			/**
			 * @brief Commit a change to the habitat's configuration made by the client.
			 * @details In order to let changes made in the habitat's configuration take effect, the
			 * changes must be reported by using this method. This will cause Hoitaja to
             * re-read its configuration and applying any changes, such as creating new cells, destroying cells etc.
			 */
			virtual void commit() = 0;

			/*****************
			 * RPC interface *
			 *****************/

			GENODE_RPC(Rpc_connect, Genode::Ram_dataspace_capability, connect);
			GENODE_RPC(Rpc_disconnect, void, disconnect);
			GENODE_RPC(Rpc_commit, void, commit);

			GENODE_RPC_INTERFACE(Rpc_connect, Rpc_disconnect, Rpc_commit);
		};
	} // namespace Shell
} // namespace Ealan

#endif

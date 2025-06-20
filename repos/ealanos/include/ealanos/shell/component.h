#ifndef __EALANOS__INCLUDE__SHELL__COMPONENT_H_
#define __EALANOS__INCLUDE__SHELL__COMPONENT_H_

#include "base/capability.h"
#include "base/entrypoint.h"
#include "base/ram_allocator.h"
#include "region_map/region_map.h"
#include <base/rpc_server.h>
#include <base/session_label.h>
#include <root/component.h>
#include <base/session_object.h>

#include <ealanos/shell/session.h>

namespace Ealan {
	class Hoitaja;

	namespace Shell {
		class Session_component : public Genode::Session_object<Ealan::Shell::Session>
		{
			private:

				Genode::Entrypoint &_ep;
				Ealan::Hoitaja     &_hoitaja;

			public:

				template <typename... ARGS>
				Session_component(Hoitaja &hoitaja, Genode::Entrypoint &ep,
				                  Genode::Session::Resources const &res, ARGS &&...args)
					: Genode::Session_object<Session>(ep, res, args...), _ep(ep), _hoitaja(hoitaja)
				{
				}

				Genode::Ram_dataspace_capability connect() override;
				void disconnect() override;

				void commit() override;
		};
    }
} // namespace Ealan

#endif
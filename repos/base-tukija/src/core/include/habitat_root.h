#ifndef __CORE_HABITAT_ROOT_H
#define __CORE_HABITAT_ROOT_H

#include "base/affinity.h"
#include "platform_generic.h"
#include <root/component.h>
#include <base/log.h>

#include <habitat_session_component.h>


namespace Core {
    class Habitat_root : public Genode::Root_component<Habitat_session_component>
    {
        private:
            Genode::Ram_allocator &_ram_alloc;
            Genode::Region_map &_local_rm;

            Genode::Rpc_entrypoint &_session_ep;

        protected:

            Habitat_session_component *_create_session(char const *args, Genode::Affinity const &affinity) override {

				Genode::log("Creating new habitat ", affinity);
				
                size_t ram_quota =
                    Arg_string::find_arg(args, "ram_quota").ulong_value(0);

                if (ram_quota < Trace::Control_area::SIZE)
                    throw Insufficient_ram_quota();

                if (!affinity.valid()) {
                    Genode::error("Invalid affinity space: ", affinity);
                    throw Genode::Service_denied();
                }

				Genode::Affinity::Location session_location =
					affinity.scale_to(Core::platform().affinity_space());

				
                return new (md_alloc()) Habitat_session_component(
                    *this->ep(),
                    session_resources_from_args(args),
                    session_label_from_args(args),
                    session_diag_from_args(args),
                    _ram_alloc,
                    _local_rm,
                    Genode::Affinity(Core::platform().affinity_space(), session_location));
            }

            void _upgrade_session(Habitat_session_component *habitat, const char *args) override
            {
                habitat->upgrade(Genode::ram_quota_from_args(args));
                habitat->upgrade(Genode::cap_quota_from_args(args));
            }

        public:
            Habitat_root(Genode::Ram_allocator &ram_alloc,
                         Genode::Region_map &local_rm,
                         Genode::Rpc_entrypoint &session_ep,
                         Genode::Allocator &md_alloc)
                : Root_component<Habitat_session_component>(&session_ep, &md_alloc), _ram_alloc(ram_alloc), _local_rm(local_rm), _session_ep(session_ep) {}
    };
}

#endif
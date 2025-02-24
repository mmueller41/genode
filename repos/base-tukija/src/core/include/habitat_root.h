#ifndef __CORE_HABITAT_ROOT_H
#define __CORE_HABITAT_ROOT_H

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

                if (!affinity.valid()) {
                    Genode::error("Invalid affinity space: ", affinity);
                    throw Genode::Service_denied();
                }

                return new (md_alloc()) Habitat_session_component(session_label_from_args(args), _session_ep, _local_rm, _ram_alloc, affinity.space());
            }

            void _upgrade_session(Habitat_session_component *, const char *) override
            {
                //habitat->upgrade(Genode::ram_quota_from_args(args));
                //habitat->upgrade(Genode::cap_quota_from_args(args));
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
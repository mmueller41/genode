#pragma once
#include <base/log.h>
#include <base/child.h>
#include <os/session_requester.h>
#include <os/session_policy.h>
#include <os/buffered_xml.h>
#include <sandbox/sandbox.h>

#include <sandbox/child.h>
#include <sandbox/service.h>
#include <sandbox/types.h>
#include <sandbox/verbose.h>
#include <sandbox/report.h>
#include <sandbox/name_registry.h>
#include <sandbox/service.h>
#include <sandbox/utils.h>
#include <sandbox/route_model.h>

#include <state_handler.h>
#include <nova/syscalls.h>
namespace Hoitaja {
    class Cell;
}

class Hoitaja::Cell : public ::Sandbox::Child
{
    private:
        State_handler &_state_handler;
        long _priority{0};

    public:
        friend class Habitat;

        Cell(Genode::Env &env,
             Genode::Allocator &alloc,
             ::Sandbox::Verbose const &verbose,
             ::Sandbox::Child::Id id,
             ::Sandbox::Report_update_trigger &report_update_trigger,
             Genode::Xml_node start_node,
             ::Sandbox::Child::Default_route_accessor &default_route_accessor,
             ::Sandbox::Child::Default_caps_accessor &default_caps_accessor,
             ::Sandbox::Name_registry &name_registry,
             ::Sandbox::Child::Ram_limit_accessor &ram_limit_accessor,
             ::Sandbox::Child::Cap_limit_accessor &cap_limit_accessor,
             ::Sandbox::Child::Cpu_limit_accessor &cpu_limit_accessor,
             ::Sandbox::Child::Cpu_quota_transfer &cpu_quota_transfer,
             ::Sandbox::Prio_levels prio_levels,
             Genode::Affinity::Space const &affinity_space,
             Genode::Affinity::Location const &location,
             Genode::Registry<::Sandbox::Parent_service> &parent_services,
             Genode::Registry<::Sandbox::Routed_service> &child_services,
             Genode::Registry<::Sandbox::Child::Local_service> &local_services,
             State_handler &state_handler)
            : ::Sandbox::Child(env, alloc, verbose, id, report_update_trigger, start_node, default_route_accessor, default_caps_accessor, name_registry, ram_limit_accessor, cap_limit_accessor, cpu_limit_accessor, cpu_quota_transfer, prio_levels, affinity_space, location, parent_services, child_services, local_services), _state_handler(state_handler)
        { 
            _priority = ::Sandbox::priority_from_xml(start_node, prio_levels);
            _priority = (_priority == 0) ? 1 : _priority;
            Genode::log("Creating new cell at Hoitaja <", name(), ">");
        }

        virtual ~Cell() { };

        struct Resources &resources() { return _resources; }

        void update_affinity(Genode::Affinity affinity) {
            //Genode::log("Updating affinity to ", affinity.location(), " in space ", affinity.space());
            _resources.affinity = affinity;
            //Genode::log("Moving CPU session ", _env.cpu_session_cap());
            _env.cpu().move(affinity.location());
            if (_child.active()) {
                _child.cpu().move(affinity.location());
                // TODO: Change topology representation
                _child.topo().reconstruct(affinity);
            }
        }

        void create_at_tukija()
        {
            Genode::log("Creating new cell <", name(), "> at Tukija at ", _resources.affinity.location());
            _child.pd().create_cell(_priority, _resources.affinity.location());
        }

        void exit(int exit_value) override
        {
            ::Sandbox::Child::exit(exit_value);
            _state_handler.handle_habitat_state(*this);
        }

        void shrink_cores(Genode::Affinity::Location &cores) {
            if (_child.active())
                _child.pd().update_cell(cores);
        }

        void grow_cores(Genode::Affinity::Location &cores) {
            if (_child.active())
                _child.pd().update_cell(cores);
        }

        void try_start() override
        {
            ::Sandbox::Child::try_start();
            while (!(_child.active()))
                __builtin_ia32_pause();
            create_at_tukija();
        }
};

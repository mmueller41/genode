/*
 * \brief   Core-specific instance of a Habitat session
 * \author  Michael Müller
 * \date    2025-02-24
 */

/*
 * Copyright (C) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 *
 * This file is part of EalánOS based on the Genode OS framework, which are distributed
 * under the terms of the GNU Affero General Public Lisense version 3.
 */
#ifndef _CORE__HABITAT_SESSION_COMPONENT_H_
#define _CORE__HABITAT_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <base/env.h>
#include <base/allocator.h>
#include <base/heap.h>
#include <base/session_label.h>

#include <habitat/session.h>
#include <tukija/syscalls.h>
#include <tukija_native_pd/client.h>
#include <pd_session/client.h>
#include <cell_component.h>


#include <nova_util.h>

namespace Core { class Habitat_session_component; }

class Core::Habitat_session_component : public Genode::Session_object<Ealan::Habitat_session>
{
    private:
        Genode::Region_map &_local_rm;
        Genode::Affinity const &_affinity;
        Genode::Session_label const &_label;
        Genode::Constrained_ram_allocator _ram_alloc;
        Genode::Sliced_heap _md_alloc;
        Genode::Rpc_entrypoint &_ep;
        Genode::List<Ealan::Cell_component> _managed_cells { };

        void _calculate_mask_for_location(Tukija::Cpuset *coreset, const Genode::Affinity::Location &loc)
        {
            const_cast<Genode::Affinity::Location&>(loc).for_each(
                [&](Genode::Affinity::Location const &location)
                {
                    unsigned kernel_cpu = Core::platform_specific().kernel_cpu_id(location);
                    coreset->set(kernel_cpu);
                });
        }

    public:
        Habitat_session_component(Genode::Rpc_entrypoint &ep, Genode::Session::Resources const &resources, Genode::Session_label const &label, Genode::Session::Diag const &diag, Genode::Ram_allocator &ram, Genode::Region_map &rm, Genode::Affinity const &affinity) : Genode::Session_object<Ealan::Habitat_session>(ep, resources, label, diag), _local_rm(rm), _affinity(affinity), _label(label), _ram_alloc(ram, _ram_quota_guard(), _cap_quota_guard()), _md_alloc(_ram_alloc, rm), _ep(ep) {}

        Ealan::Cell_capability create_cell(Genode::Capability<Genode::Pd_session> pd_cap, [[maybe_unused]] Genode::Affinity &affinity, Genode::uint16_t prio, Genode::Session_label const &label, bool is_brick) override {

            Ealan::Cell_component *cell = new (_md_alloc) Ealan::Cell_component(pd_cap, prio, affinity, _ep, _local_rm, label, is_brick);

            _managed_cells.insert(cell);

            return cell->cap();
        }

        Genode::Affinity affinity() override
        {
            Genode::Affinity::Space const &core_space = Core::platform().affinity_space();

            return Genode::Affinity(core_space, _affinity.location());
        }

        void groom() override
        {
            Genode::List<Ealan::Cell_component> dead_cells{};
            for (Ealan::Cell_component *cell = _managed_cells.first(); cell != nullptr; cell = cell->next())
            {
                if (cell->is_dead())
                    dead_cells.insert(cell);
            }

            Genode::size_t count = 0;
            for (Ealan::Cell_component *cell = dead_cells.first(); cell != nullptr;)
            {
                Ealan::Cell_component *dead_cell = cell;
                cell = dead_cell->next();
                _managed_cells.remove(dead_cell);
                destroy(_md_alloc, dead_cell);
                count++;
            }

            Genode::log("Removed ", count, " dead cells from habitat");
        }
};

#endif
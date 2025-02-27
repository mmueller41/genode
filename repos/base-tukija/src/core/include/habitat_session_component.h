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

class Core::Habitat_session_component : public Genode::Rpc_object<Ealan::Habitat_session, Habitat_session_component>
{
    private:
        Genode::Region_map &_local_rm;
        Genode::Affinity const &_affinity;
        Genode::Session_label const &_label;
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
        Habitat_session_component(Genode::Session_label const &label, Genode::Rpc_entrypoint &session_ep, Genode::Region_map &rm, Genode::Ram_allocator &alloc, Genode::Affinity const &affinity) :  _local_rm(rm), _affinity(affinity), _label(label), _md_alloc(alloc, rm), _ep(session_ep) {}

        Ealan::Cell_capability create_cell(Genode::Capability<Genode::Pd_session> pd_cap, [[maybe_unused]] Genode::Affinity &affinity, Genode::uint16_t prio, Genode::Session_label const &label) override {

            Ealan::Cell_component *cell = new (_md_alloc) Ealan::Cell_component(pd_cap, prio, affinity, _ep, _local_rm, label);

            _managed_cells.insert(cell);

            return cell->cap();
        }

        Genode::Affinity affinity() override
        {
            Genode::Affinity::Space const &core_space = Core::platform().affinity_space();

            return Genode::Affinity(core_space, _affinity.location());
        }
};

#endif
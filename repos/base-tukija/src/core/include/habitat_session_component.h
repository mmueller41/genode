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

#include "base/affinity.h"
#include "base/ram_allocator.h"
#include "base/stdint.h"
#include "dataspace_component.h"
#include "platform_generic.h"
#include "region_map/region_map.h"
#include "tukija/stdint.h"
#include "tukija/syscall-generic.h"
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
#include <tukija/cap_map.h>

#include <nova_util.h>

namespace Core { class Habitat_session_component; }

class Core::Habitat_session_component : public Genode::Session_object<Ealan::Habitat_session>
{
    private:
        Genode::Region_map &_local_rm;
        Genode::Affinity const _affinity;
        Genode::Session_label const &_label;
        Genode::Constrained_ram_allocator _ram_alloc;
        Genode::Sliced_heap _md_alloc;
        Genode::Rpc_entrypoint &_ep;
		Genode::List<Ealan::Cell_component> _managed_cells{};
		Genode::addr_t                      _id_base;
		Tukija::Habitat_info_page *haip{};

		Genode::addr_t _sel() const { return _id_base; }
		

        void _calculate_mask_for_location(Tukija::Cpuset *coreset, const Genode::Affinity::Location &location)
        {
            const_cast<Genode::Affinity::Location&>(location).for_each(
                [&](Genode::Affinity::Location const &location)
                {
                    unsigned kernel_cpu = Core::platform_specific().kernel_cpu_id(location);
                    coreset->set(kernel_cpu);
                });
		}

		Habitat_session_component(const Habitat_session_component &);

		Habitat_session_component& operator=(const Habitat_session_component&);

    public:

		Habitat_session_component(Genode::Rpc_entrypoint           &ep,
		                          Genode::Session::Resources const &resources,
		                          Genode::Session_label const      &label,
		                          Genode::Session::Diag const &diag, Genode::Ram_allocator &ram,
		                          Genode::Region_map &rm, Genode::Affinity const &affinity)
			: Genode::Session_object<Ealan::Habitat_session>(ep, resources, label, diag),
			  _local_rm(rm), _affinity(affinity), _label(label),
			  _ram_alloc(ram, _ram_quota_guard(), _cap_quota_guard()), _md_alloc(_ram_alloc, rm),
			  _ep(ep), _id_base(cap_map().insert(1))
		{

            Core::platform().region_alloc().alloc_aligned(Tukija::PAGE_SIZE_BYTE, Tukija::PAGE_SIZE_LOG2).with_result([&](void *ptr) { haip = static_cast<Tukija::Habitat_info_page*>(ptr); }, [&](Genode::Range_allocator::Alloc_error) { haip = nullptr; });
			
			if (!haip) { Genode::error("Failed to allocate Habitat Info Page"); }
			Tukija::create_habitat(_sel(), reinterpret_cast<Tukija::mword_t>(haip));

            _calculate_mask_for_location(&haip->reserved_cores, _affinity.location());

            Genode::log("Created habitat: ", haip->reserved_cores);
		}

        Ealan::Cell_capability create_cell(Genode::Capability<Genode::Pd_session> pd_cap, [[maybe_unused]] Genode::Affinity &affinity, Genode::uint16_t prio, Genode::Session_label const &label, bool is_brick) override {

            Genode::log("Habitat ", _affinity);
			Genode::log(haip->reserved_cores);
			
			Genode::Affinity::Location location = affinity.location().transpose(
				_affinity.location().xpos(), _affinity.location().ypos());

			Genode::Affinity session_affinity(Core::platform().affinity_space(), location);

			Genode::log(label, ": ", affinity, "->", session_affinity);
			
            Ealan::Cell_component *cell = new (_md_alloc) Ealan::Cell_component(pd_cap, prio, session_affinity, _ep, _local_rm, label, is_brick, _sel(), _affinity);

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
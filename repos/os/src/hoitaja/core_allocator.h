/*
 * \brief  Hoitaja — Core Allocator
 * \author Michael Müller, Norman Feske (Init)
 * \date   2023-04-20
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 * Copyright (C) 2023 Michael Müller, Osnabrück University
 *
 * This file is part of EalánOS, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once
/* Genode includes */
#include <sandbox/child.h>
#include <sandbox/library.h>
#include <sandbox/utils.h>
#include <sandbox/types.h>
#include <util/string.h>

#include <nova/syscalls.h>

/** Hoitaja includes **/
#include "load_controller.h"
#include "cell_controller.h"
#include <cell.h>

namespace Hoitaja 
{
    class Core_allocator;
}

class Hoitaja::Core_allocator
{
    private:
        Genode::Affinity::Space &_affinity_space;

        ::Sandbox::Prio_levels &_prio_levels;

        double _resource_coeff; // Coefficient used for calculating resource shares

    public:
        inline unsigned int _calculate_resource_share(long priority) {
            double ref_share = static_cast<double>(_affinity_space.total()) / _resource_coeff;
            return static_cast<unsigned int>((1.0 / static_cast<double>(priority)) * ref_share);
        }

        Core_allocator(Genode::Affinity::Space &affinity_space, ::Sandbox::Prio_levels prio_levels) : _affinity_space(affinity_space), _prio_levels(prio_levels), _resource_coeff(0.0)
        {
            Genode::log("Created core allocator for ", affinity_space.total(), " cores and ", prio_levels.value, " priorities.");
            Nova::create_habitat(0, affinity_space.total());
        }

        Genode::Affinity::Location allocate_cores_for_cell(Genode::Xml_node const &start_node)
        {
            // Calculate affinity from global affinity space and priority
            long priority = ::Sandbox::priority_from_xml(start_node, _prio_levels);
            priority = (priority == 0) ? 1 : priority;
            _resource_coeff += (1.0/static_cast<double>(priority)); // treat priority 0 same as 1, to avoid division by zero here

            unsigned int cores_share = _calculate_resource_share(priority);
            
         
            return Genode::Affinity::Location( _affinity_space.total()-cores_share, 0, cores_share, 1 ); /* always use the core_share last cores, for now */
        }

        void free_cores_from_cell(Cell &cell)
        {
            /* Remove cell's coefficient from the global resource coefficient.
             * This is necessary in order to be able to redistribute the freed resources correctly. We do not trigger the redistribution itself here, because the child has not been fully destroyed yet, thus its resources might still be occupied at this point. */
            _resource_coeff -= 1.0 / static_cast<double>(cell.resources().priority);
        }

        /**
         * @brief Update core allocations for cells reported by Cell controller
         * 
         */
        void update(Hoitaja::Cell &cell, int *xpos) {
            if (cell.abandoned())
                return;
            Cell::Resources resources = cell.resources();
            long priority = (resources.priority == 0)? 1 : resources.priority;

            unsigned int cores_share = _calculate_resource_share(priority);
            unsigned int cores_to_reclaim = resources.affinity.location().width() * resources.affinity.location().height() - cores_share;

            cores_to_reclaim = (static_cast<int>(cores_to_reclaim) < 0) ? 0 : cores_to_reclaim;

            if (*xpos - cores_share == 0) {
                cores_share--; // Save one core for Hoitaja
            }

            Genode::Affinity::Location location(*xpos - cores_share, resources.affinity.location().ypos(), cores_share, resources.affinity.location().height());
            
            if (resources.affinity.location() != location) { // Only update, if location has actually changed
                cell.update_affinity(Genode::Affinity(resources.affinity.space(), location));
            }

            if (location.width() > resources.affinity.location().width()) {
                cell.grow_cores(location);
            }

            *xpos = location.xpos();
            // TODO: Update affinity of existing sessions for cell
            // TODO: Send yield request to cell

            if (cores_to_reclaim > 0) {
                log("Need to reclaim ", cores_to_reclaim, " cores from ", cell.name());
                cell.shrink_cores(location);
            }
        }

};
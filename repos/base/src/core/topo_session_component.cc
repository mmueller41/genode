/*
 * \brief   Topology session interface
 * \author  Michael Müller
 * \date    2022-10-06
 *
 * A topology session stores the component's view on the hardware topology, i.e. it's location within the NUMA topology.
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is part of EalánOS which is based on the Genode OS framework
 * released under the terms of the GNU Affero General Public License version 3.
 */
#include <topo_session_component.h>
#include <platform_generic.h>
#include <platform.h>
#include <base/log.h>

using namespace Genode;

Topo_session_component::Topo_session_component(Rpc_entrypoint &session_ep,
                                               Resources const &resources,
                                               Label const &label,
                                               Diag const &diag,
                                               Ram_allocator &ram_alloc,
                                               Region_map &local_rm,
                                               Affinity affinity)
    : Session_object(session_ep, resources, label, diag),
      _affinity(affinity),
      _md_alloc(ram_alloc, local_rm),
      _node_count(0)
{
    construct();
}

void Topo_session_component::construct()
{
    Affinity::Location location = _affinity.location();
    const unsigned height = location.height();
    unsigned width = location.width();
    unsigned curr_node_id = 0;
    Topology::Numa_region *node_created = new (_md_alloc) Topology::Numa_region[64]();

    Genode::log("[", label(), "] Creating new topology model of size ", width, "x", height);

    for (unsigned x = 0; x < width; x++)
    {
        for (unsigned y = 0; y < height; y++)
        {
            /* Map component's affinity matrix to its position in the affinity space.
             * In order to get the correct physical CPU id for a coordinate in the affinity matrix of a component,
             * we need the global coordination for it relative to the whole affinity space.
             * But, every component's maintains a local view on its affinity matrix starting by (0,0), since
             * affinity locations can have arbitrary coordinates in the affinity space, we need to transpose the 
             * component's affinity matrix to the global view of the affinity space. */
            Affinity::Location loc = location.transpose(x, y); 
            unsigned cpu_id = platform_specific().kernel_cpu_id(loc);
            unsigned native_id = platform_specific().domain_of_cpu(cpu_id);

            log("[", label(), "] CPU (", x, "x", y, ") is native CPU ", cpu_id, " on node ", native_id);

            if (node_created[native_id].core_count() == 0)
            {
                _nodes[curr_node_id] = _node_affinities[x][y] = Topology::Numa_region(curr_node_id, native_id);
                _node_affinities[x][y].increment_core_count();
                node_created[native_id] = _node_affinities[x][y];
                log("[", label(), "] Found new native NUMA region ", native_id, " for CPU (", x, "x", y, ")");
                _node_count++;
                curr_node_id++;
            }
            else
            {
                (_node_affinities[x][y] = node_created[native_id]).increment_core_count();
                _nodes[curr_node_id].increment_core_count();
            }
        }
    }

}
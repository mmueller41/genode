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

using namespace Genode;

Topo_session_component::Topo_session_component(Rpc_entrypoint &session_ep,
                                               Resources const &resources,
                                               Label const &label,
                                               Diag const &diag,
                                               Ram_allocator &ram_alloc,
                                               Region_map &local_rm,
                                               Affinity &affinity)
: 
    Session_object(session_ep, resources, label, diag),
    _md_alloc(ram_alloc, local_rm),
    _affinity(affinity) 
{
    const unsigned height = affinity.space().height();
    unsigned width = affinity.space().width();
    unsigned curr_node_id = 0; 
    Node *node_created[] = new (_md_alloc) Node *[64]();

    _node_affinities = new (_md_alloc) Node **[width];
    

    for (unsigned x = 0; x < width; x++) {
        _node_affinities[x] = new (_md_alloc) Node *[height];
        for (unsigned y = 0; y < height; y++) {
            
            Affinity::Location loc = Affinity::Location(x, y);
            unsigned cpu_id = platform_specific().kernel_cpu_id(loc);
            unsigned native_id = platform_specific().domain_of_cpu(cpu_id);
            
            if (!node_created[node_id]) {
                _node_affinities[x][y] = new (_md_alloc) Node(curr_node_id, native_id);
                _node_affinities[x][y]->increment_core_count();
                _node_count++;
            } else {
                (_node_affinities[x][y] = node_created[native_id])->increment_core_count();
            }
        }
    }
}

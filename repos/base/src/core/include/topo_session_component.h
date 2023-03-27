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

#pragma once

/* Genode includes */
#include <base/session_object.h>
#include <base/affinity.h>
#include <base/heap.h>
#include <topo_session/topo_session.h>
#include <platform.h>
#include <topo_session/node.h>

namespace Genode {
    class Topo_session_component;
}

class Genode::Topo_session_component : public Session_object<Topo_session>
{
    private:
        Genode::Affinity &_affinity;
        Sliced_heap _md_alloc;
        
        Topology::Numa_region _node_affinities[Genode::Platform::MAX_SUPPORTED_CPUS][Genode::Platform::MAX_SUPPORTED_CPUS];
        unsigned _node_count;
        Topology::Numa_region _nodes[64];

    public:
        Topo_session_component(Rpc_entrypoint &session_ep,
                               Resources const &resources,
                               Label const &label,
                               Diag const &diag,
                               Ram_allocator &ram_alloc,
                               Region_map &local_rm,
                               Affinity &affinity
                               );


        /**
         * @brief Topology session interface
         */

        Topology::Numa_region node_affinity_of(Affinity::Location const &loc) override
        {
            return _node_affinities[loc.xpos()][loc.ypos()];
        }

        Topology::Numa_region node_at_id(unsigned numa_id) override
        {
            return _nodes[numa_id];
        }

        unsigned node_count() override
        {
            return _node_count;
        }   
};

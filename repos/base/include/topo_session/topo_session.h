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

#include <session/session.h>
#include <base/affinity.h>

namespace Genode {

    struct Topo_session;
    struct Topo_session_client;
}

namespace Topology
{
    struct Numa_region;
} // namespace EalanOS

struct Genode::Topo_session : Session 
{
    /**
     * \nooapi
     * 
     */
    static const char *service_name() { return "TOPO"; }

    enum
    {
        CAP_QUOTA = 2
    };

    typedef Topo_session_client Client;

    virtual ~Topo_session() { }

    virtual Topology::Numa_region node_affinity_of(Affinity::Location const &) = 0;
    virtual Topology::Numa_region node_at_id(unsigned node_id) = 0;
    virtual unsigned node_count() = 0;
    virtual void reconstruct(const Affinity) = 0;
    virtual unsigned phys_id(Affinity::Location const &) = 0;
    virtual Affinity::Space const global_affinity_space() = 0;

    GENODE_RPC(Rpc_node_affinity, Topology::Numa_region, node_affinity_of, Affinity::Location const &);
    GENODE_RPC(Rpc_node_id, Topology::Numa_region, node_at_id, unsigned);
    GENODE_RPC(Rpc_node_count, unsigned, node_count);
    GENODE_RPC(Rpc_reconstruct, void, reconstruct, Affinity);
    GENODE_RPC(Rpc_phys_id, unsigned, phys_id, Affinity::Location const &);
    GENODE_RPC(Rpc_total_core_count, Affinity::Space const, global_affinity_space);

    GENODE_RPC_INTERFACE(Rpc_node_affinity, Rpc_node_id, Rpc_node_count, Rpc_reconstruct, Rpc_phys_id, Rpc_total_core_count);
};
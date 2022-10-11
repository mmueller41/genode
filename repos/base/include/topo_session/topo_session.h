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
    struct Node;
}

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

    virtual Node *node_affinity_of(Affinity::Location &) = 0;
    virtual unsigned node_count() = 0;

    GENODE_RPC(Rpc_node_affinity, Node*, node_affinity_of, Affinity::Location &);
    GENODE_RPC(Rpc_node_count, unsigned, node_count);

    GENODE_RPC_INTERFACE(Rpc_node_affinity, Rpc_node_count);
};
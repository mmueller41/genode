/*
 * \brief   Client-side topology session interface
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

#include <topo_session/capability.h>
#include <topo_session/node.h>
#include <base/rpc_client.h>
#include <base/affinity.h>

namespace Genode {
    struct Topo_session_client;
    struct Node;
}

struct Genode::Topo_session_client : Rpc_client<Topo_session>
{
    explicit Topo_session_client(Topo_session_capability session)
    : Rpc_client<Topo_session>(session) { }

    Node *node_affinity_of(Affinity::Location &loc) override {
        return call<Rpc_node_affinity>(loc);
    }

    unsigned node_count() override {
        return call<Rpc_node_count>();
    }
};
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

#include <topo_session/client.h>
#include <topo_session/node.h>
#include <base/connection.h>

namespace Genode {
    struct Topo_connection;
}

struct Genode::Topo_connection : Connection<Topo_session>, Topo_session_client
{
    enum
    {
        RAM_QUOTA = 8192
    };

    Topo_connection(Env &env, const char *label = "", Affinity const &affinity = Affinity()) 
    :
        Connection<Topo_session>(env, 
                                session(env.parent(), affinity, "ram_quota=%u, cap_quota=%u, label=\"%s\"", RAM_QUOTA, CAP_QUOTA, label)),
    Topo_session_client(cap()) {}

    Node *node_affinity_of(Affinity::Location &loc) override {
        return Topo_session_client::node_affinity_of(loc);
    }

    unsigned node_count() override {
        return Topo_session_client::node_count();
    }
};
/*
 * \brief   Topology session client that upgrades its session quota on demand
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

#include <util/retry.h>
#include <topo_session/client.h>

#include <base/internal/upgradeable_client.h>

namespace Genode {
    struct Expanding_topo_session_client;
}

struct Genode::Expanding_topo_session_client : Upgradeable_client<Genode::Topo_session_client>
{
    Expanding_topo_session_client(Parent &parent, Genode::Topo_session_capability cap, Parent::Client::Id id) 
    :
        Upgradeable_client<Genode::Topo_session_client>
            (parent, static_cap_cast<Genode::Topo_session_client::Rpc_interface>(cap), id)
    { }

    Topology::Numa_region node_affinity_of(Affinity::Location const &loc) override 
    {
        return retry<Out_of_ram>(
            [&]()
            {
                return retry<Out_of_caps>(
                    [&]()
                    {
                        return Topo_session_client::node_affinity_of(loc);
                    },
                    [&]()
                    { upgrade_caps(2); });
            },
            [&]()
            { upgrade_ram(8 * 1024); });
    }
};
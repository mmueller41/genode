/*
 * \brief   Representation of a NUMA node
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

#include <util/list.h>

namespace Genode {
    struct Node;
}

struct Genode::Node : List<Node>::Element 
{
    /* ID presented to component */
    unsigned _id;

    unsigned _core_count;
    List<Node> neighbours;

    /* Physical NUMA node ID */
    unsigned _native_id;

    Node(unsigned id, unsigned native_id) : _id(id), _core_count(0), neighbours(), _native_id(native_id) {}

    unsigned native_id() { return _native_id; }
    unsigned id() { return _id; }
    unsigned core_count() { return _core_count; }
    void core_count(unsigned count) { _core_count = count; }
    void increment_core_count() { _core_count++; }
};
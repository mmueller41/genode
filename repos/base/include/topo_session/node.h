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

namespace Topology {
    struct Numa_region;
}

struct Topology::Numa_region : Genode::List<Topology::Numa_region>::Element 
{
    /* ID presented to component */
    unsigned _id;

    unsigned _core_count;
    Genode::List<Topology::Numa_region> neighbours;

    /* Physical NUMA node ID */
    unsigned _native_id;

    Numa_region() : _id(0), _core_count(0), neighbours(), _native_id(0) { }
    Numa_region(unsigned id, unsigned native_id) : _id(id), _core_count(0), neighbours(), _native_id(native_id) {}
    Numa_region(Numa_region &copy) : _id(copy.id()), _core_count(copy.core_count()), neighbours(), _native_id(copy.native_id()) {
    }

    unsigned native_id() { return _native_id; }
    unsigned id() { return _id; }
    unsigned core_count() { return _core_count; }
    void core_count(unsigned count) { _core_count = count; }
    void increment_core_count() { _core_count++; }
    Numa_region &operator=(const Numa_region &copy) {
        if (this == &copy)
            return *this;

        this->_id = copy._id;
        this->_core_count = copy._core_count;
        this->_native_id = copy._native_id;

        /* At the moment, we do not copy the list of neighbours, as it is not used by any our applications. */
        /* TODO: Copy list onf neighbours, as soons as any application is going to use that information. */

        return *this;
    }
};
/*
 * \brief   Cell-session interface
 * \author  Michael Müller
 * \date    2025-02-17
*/

/*
 * Copyright (C) 2025 Michael Müller, Osnabrück University
 *
 * This file is part of the EalánOS research operating system, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
*/

#ifndef __EALANOS_HABITAT_SESSION_H_
#define __EALANOS_HABITAT_SESSION_H_

/* Genode includes */
#include <base/rpc_args.h>
#include <session/session.h>
#include <pd_session/pd_session.h>
#include <cell/cell.h>
#include <base/affinity.h>

namespace Ealan { struct Habitat_session; }

struct Ealan::Habitat_session : Genode::Session
{
    static const char *service_name() { return "Habitat"; }

    static constexpr Genode::size_t MAX_NUM_CELLS = 256;
     // Same as Core::Platform::MAX_SUPPORTED_CPUS;
    static constexpr unsigned CAP_QUOTA = MAX_NUM_CELLS;
    static constexpr Genode::size_t   RAM_QUOTA = MAX_NUM_CELLS*32*1024;

    /**
     * Attach cell info page to the cells virtual memory space
     */
    virtual Cell_capability create_cell(Genode::Capability<Genode::Pd_session> pd, Genode::Affinity &affinity, Genode::uint16_t prio, Genode::Session_label const &label) = 0;

    /**
     * @brief Clean up the habitat by removing terminated cells and freeing their memory
     * 
     */
    virtual void groom() = 0;

    virtual Genode::Affinity affinity() = 0;

    GENODE_RPC_THROW(Rpc_create_cell, Cell_capability, create_cell, GENODE_TYPE_LIST(Ealan::Cell::Cell_creation_error), Genode::Capability<Genode::Pd_session>, Genode::Affinity &, Genode::uint16_t, Genode::Session_label const &);
    GENODE_RPC(Rpc_affinity, Genode::Affinity, affinity);
    GENODE_RPC(Rpc_groom, void, groom);

    GENODE_RPC_INTERFACE(Rpc_create_cell, Rpc_affinity, Rpc_groom);
};
#endif
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

namespace Ealan { struct Habitat_session; }

struct Ealan::Habitat_session : Genode::Session
{
    static const char *service_name() { return "Habitat"; }

    enum { CAP_QUOTA = 1, RAM_QUOTA = 1024 };

    /**
     * Attach cell info page to the cells virtual memory space
     */
    virtual void create_cell(Genode::Capability<Genode::Pd_session> pd, Genode::Affinity &affinity, Genode::uint16_t prio) = 0;

    GENODE_RPC(Rpc_create_cell, void, create_cell, Genode::Capability<Genode::Pd_session>, Genode::Affinity&, Genode::uint16_t);
    GENODE_RPC_INTERFACE(Rpc_create_cell);
};
#endif
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

#ifndef __EALANOS_HABITAT_CLIENT_H_
#define __EALANOS_HABITAT_CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>

#include <habitat/session.h>

namespace Ealan { struct Habitat_client; 
using Habitat_capability = Genode::Capability<Ealan::Habitat_session>;
}

struct Ealan::Habitat_client : Genode::Rpc_client<Ealan::Habitat_session>
{
    explicit Habitat_client(Habitat_capability session) : Rpc_client<Habitat_session>(session) {}

    void create_cell(Genode::Capability<Genode::Pd_session> pd, Genode::Affinity &affinity, Genode::uint16_t prio) override {
        call<Rpc_create_cell>(pd, affinity, prio);
    }
};
#endif
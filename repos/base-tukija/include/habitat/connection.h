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
#ifndef __EALANOS_HABITAT_CONNECTIOn_H_
#define __EALANOS_HABITAT_CONNECTIOn_H_

/* Genode includes */
#include <base/rpc_args.h>
#include <session/session.h>

/* Ealan includes */
#include <habitat/client.h>
#include <base/connection.h>

namespace Ealan { struct  Habitat_connection; }

struct Ealan::Habitat_connection : Genode::Connection<Ealan::Habitat_session>, Habitat_client
{
    Habitat_connection(Genode::Env &env, Genode::Affinity &affinity, Label const &label = Label()) 
    : Connection<Habitat_session>(env, label, Genode::Ram_quota { RAM_QUOTA }, affinity, Args("")), Habitat_client(cap()) {}

    void create_cell(Genode::Capability<Genode::Pd_session> pd_cap, Genode::Affinity &affinity, Genode::uint16_t prio) override {
        Habitat_client::create_cell(pd_cap, affinity, prio);
    }
};

#endif

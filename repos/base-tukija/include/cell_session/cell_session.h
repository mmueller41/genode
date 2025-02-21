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

/* Genode includes */
#include <base/rpc_args.h>
#include <session/session.h>

namespace Ealan { struct Cell_session; }

struct Ealan::Cell_session : Genode::Session
{
    static const char *service_name() { return "Cell"; }

    enum { CAP_QUOTA = 1 };

    /**
     * Attach cell info page to the cells virtual memory space
     */
    virtual void attach_cip(Dataspace_capability ds, attr_t) = 0;
};
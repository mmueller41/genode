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

#ifndef _INCLUDE__CELL__SESSION_H_
#define _INCLUDE__CELL__SESSION_H_

/* Genode includes */
#include <base/affinity.h>
#include <base/capability.h>
#include <base/rpc_args.h>
#include <session/session.h>

namespace Ealan { struct Cell;
    using Cell_capability = Genode::Capability<Cell>;
}

struct Ealan::Cell : Genode::Interface
{
    enum { CAP_QUOTA = 1 };

    virtual void update(Genode::Affinity &affinity) = 0;
    virtual bool is_brick() = 0;

    GENODE_RPC(Rpc_update, void, update, Genode::Affinity &);
    GENODE_RPC(Rpc_is_brick, bool, is_brick);

    GENODE_RPC_INTERFACE(Rpc_update, Rpc_is_brick);
};
#endif /* _INCLUDE__CELL__SESSION_H_ */
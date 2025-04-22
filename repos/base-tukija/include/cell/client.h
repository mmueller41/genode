/*
 * \brief   Client-side Cell interface
 * \author  Michael Müller
 * \date    2025-02-24
 */

/*
 * Copyright (C) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 *
 * This file is part of EalánOS based on the Genode OS framework, which are distributed
 * under the terms of the GNU Affero General Public Lisense version 3.
 */

#ifndef _INCLUDE__CELL__CLIENT_H_
#define _INCLUDE__CELL__CLIENT_H_

#include <cell/cell.h>
#include <base/rpc_client.h>

namespace Ealan {
    struct Cell_client;
}

struct Ealan::Cell_client : Genode::Rpc_client<Cell>
{
    explicit Cell_client(Cell_capability cap) 
    : Rpc_client<Cell>(cap) {}

    void update(Genode::Affinity &affinity) override {
        call<Rpc_update>(affinity);
    }

    bool is_brick() override {
        return call<Rpc_is_brick>();
    }

    void die() override {
        call<Rpc_die>();
    }
};

#endif /* _INCLUDE__CELL__CLIENT_H_ */
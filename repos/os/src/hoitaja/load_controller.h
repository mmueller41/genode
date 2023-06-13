/*
 * \brief  Hoitaja — Load Controller
 * \author Michael Müller, Norman Feske (Init)
 * \date   2023-04-20
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 * Copyright (C) 2023 Michael Müller, Osnabrück University
 *
 * This file is part of EalánOS, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <trace_session/connection.h>
#include <base/affinity.h>

namespace Hoitaja {
    class Load_controller;
}

class Hoitaja::Load_controller
{
    public:
        unsigned short *cpu_loads();
        Genode::Affinity::Location *idle_cores();
};
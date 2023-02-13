/*
 * \brief   Topo-session capability type
 * \author  Michael Müller
 * \date    2022-10-06
 */

/*
 * Copyright (C) 2022 Michael Müller
 * 
 * This file is part of EalanOS, witch is based on Genode OS framework
 * distributed under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/capability.h>
#include <topo_session/topo_session.h>

namespace Genode
{
    typedef Capability<Topo_session> Topo_session_capability;   
} // namespace Genode
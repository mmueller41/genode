/*
 * \brief  Genode native lwIP initalization
 * \author Emery Hemingway
 * \date   2017-08-21
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__MXIP__GENODE_INIT_H_
#define _INCLUDE__MXIP__GENODE_INIT_H_

#include <timer/timeout.h>
#include <base/allocator.h>

namespace Mxip {
	void mxip_init(Genode::Allocator &heap, ::Timer::Connection &timer);
}

#endif

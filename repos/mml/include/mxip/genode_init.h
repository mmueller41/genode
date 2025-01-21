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

#ifndef _INCLUDE__LWIP__GENODE_INIT_H_
#define _INCLUDE__LWIP__GENODE_INIT_H_

#include <timer/timeout.h>
#include <mx/memory/dynamic_size_allocator.h>

namespace Mxip {
	void mxip_init(mx::memory::dynamic::Allocator &heap, ::Timer::Connection &timer);

	Genode::Mutex &mutex();
}

#endif

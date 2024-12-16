/*
 * \brief  OKL4-specific implementation of the IO_MEM session interface
 * \author Norman Feske
 * \date   2009-03-29
 *
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <io_mem_session_component.h>


using namespace Core;


void Io_mem_session_component::_unmap_local(addr_t, size_t, addr_t) { }


Io_mem_session_component::Dataspace_attr Io_mem_session_component::_map_local(addr_t const base,
                                                                              size_t const size,
                                                                              addr_t const req_base)
{
	return Dataspace_attr(size, 0, base, _cacheable, req_base);
}

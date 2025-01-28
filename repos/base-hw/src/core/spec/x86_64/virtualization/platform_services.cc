/*
 * \brief  Platform specific services for x86
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2012-10-26
 */

/*
 * Copyright (C) 2012-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>

/* core includes */
#include <platform.h>
#include <platform_services.h>
#include <vm_root.h>
#include <io_port_root.h>


/*
 * Add x86 specific ioport and virtualization service
 */
void Core::platform_add_local_services(Rpc_entrypoint         &ep,
                                       Sliced_heap            &sliced_heap,
                                       Registry<Service>      &local_services,
                                       Trace::Source_registry &trace_sources,
                                       Ram_allocator          &core_ram,
                                       Region_map             &core_rm,
                                       Range_allocator        &io_port_ranges)
{
	static Io_port_root io_port_root(io_port_ranges, sliced_heap);

	static Vm_root vm_root(ep, sliced_heap, core_ram, core_rm, trace_sources);

	static Core_service<Session_object<Vm_session>> vm_service(local_services, vm_root);

	static Core_service<Io_port_session_component> io_port_ls(local_services, io_port_root);
}

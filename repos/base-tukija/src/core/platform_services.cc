/*
 * \brief  Platform specific services for Tukija
 * \author Alexander Boettcher
 * \author Michael Müller
 * \date   2025-02-14
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_services.h>
#include <vm_root.h>
#include <io_port_root.h>
#include <base/log.h>
#include <habitat_root.h>

/*
 * Add x86 specific services 
 */
void Core::platform_add_local_services(Rpc_entrypoint         &ep,
                                       Sliced_heap            &heap,
                                       Registry<Service>      &services,
                                       Trace::Source_registry &trace_sources,
                                       Ram_allocator          &core_ram,
                                       Region_map             &core_rm,
                                       Range_allocator        &io_port_ranges)
{
	static Vm_root vm_root(ep, heap, core_ram, core_rm, trace_sources);
	static Core_service<Vm_session_component> vm(services, vm_root);

	static Rpc_entrypoint habitat_entry{nullptr, 20*1024, "habitat_entry", Affinity::Location()};
	static Io_port_root io_root(io_port_ranges, heap);
    static Habitat_root habitat_root(core_ram, core_rm, habitat_entry, heap);

    log("Adding Tukija specific services");
    static Core_service<Io_port_session_component> io_port(services, io_root);
    static Core_service<Habitat_session_component> habitat(services, habitat_root);
}

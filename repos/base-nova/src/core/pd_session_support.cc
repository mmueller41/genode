/*
 * \brief  Extension of core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core */
#include <pd_session_component.h>
#include <assertion.h>

using namespace Genode;


bool Pd_session_component::assign_pci(addr_t pci_config_memory, uint16_t bdf)
{
	uint8_t res = Nova::NOVA_PD_OOM;
	do {
		res = Nova::assign_pci(_pd->pd_sel(), pci_config_memory, bdf);
	} while (res == Nova::NOVA_PD_OOM &&
	         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
	                                                   _pd->pd_sel(),
	                                                   "core", "ep",
	                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

	return res == Nova::NOVA_OK;
}


void Pd_session_component::map(addr_t virt, addr_t size)
{
	Genode::addr_t const  pd_core   = platform_specific().core_pd_sel();
	Platform_pd          &target_pd = *_pd;
	Genode::addr_t const  pd_dst    = target_pd.pd_sel();
	Nova::Utcb           &utcb      = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());

	auto lambda = [&] (Region_map_component *region_map,
	                   Rm_region            *region,
	                   addr_t const          ds_offset,
	                   addr_t const          region_offset,
	                   addr_t const          dst_region_size) -> addr_t
	{
		Dataspace_component * dsc = region ? &region->dataspace() : nullptr;
		if (!dsc) {
			struct No_dataspace{};
			throw No_dataspace();
		}
		if (!region_map) {
			ASSERT_NEVER_CALLED;
		}

		Mapping mapping = Region_map_component::create_map_item(region_map,
		                                                        *region,
		                                                        ds_offset,
		                                                        region_offset,
		                                                        *dsc, virt,
		                                                        dst_region_size);

		/* asynchronously map memory */
		uint8_t err = Nova::NOVA_PD_OOM;
		do {
			utcb.set_msg_word(0);

			bool res = utcb.append_item(nova_src_crd(mapping), 0, true, false,
			                            false,
			                            mapping.dma_buffer,
			                            mapping.write_combined);

			/* one item ever fits on the UTCB */
			(void)res;

			err = Nova::delegate(pd_core, pd_dst, nova_dst_crd(mapping));

		} while (err == Nova::NOVA_PD_OOM &&
		         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
		                                                   _pd->pd_sel(),
		                                                   "core", "ep",
		                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

		addr_t const map_size = 1UL << mapping.size_log2;
		addr_t const mapped = mapping.dst_addr + map_size - virt;

		if (err != Nova::NOVA_OK) {
			error("could not map memory ",
			      Hex_range<addr_t>(mapping.dst_addr, map_size) , " "
			      "eagerly error=", err);
		}

		return mapped;
	};

	try {
		while (size) {
			addr_t mapped = _address_space.apply_to_dataspace(virt, lambda);
			virt         += mapped;
			size          = size < mapped ? size : size - mapped;
		}
	} catch (...) {
		error(__func__, " failed ", Hex(virt), "+", Hex(size));
	}
}

void _calculate_mask_for_location(Nova::mword_t *core_mask, const Affinity::Location &loc)
{
	for (unsigned y = loc.ypos(); y < loc.ypos() + loc.height(); y++)
	{
		for (unsigned x = loc.xpos(); x < loc.xpos()+loc.width(); x++)
		{
			unsigned kernel_cpu = platform_specific().kernel_cpu_id(Affinity::Location(x, y, loc.width(), loc.height()));
			unsigned i = kernel_cpu / (sizeof(Nova::mword_t) * 8);
			unsigned b = kernel_cpu % (sizeof(Nova::mword_t) * 8);
			core_mask[i] |= (1UL << b);

			Genode::log("core_mask[", i, "]=", core_mask[i], " i=", i, "b=", b, "kernel_cpu=", kernel_cpu);
		}
	}
}


void Pd_session_component::create_cell(long prioritiy, const Affinity::Location &loc)
{
	Nova::uint8_t err = Nova::NOVA_OK;
	unsigned num_cpus = platform_specific().MAX_SUPPORTED_CPUS;
	unsigned num_vect = num_cpus / (sizeof(Nova::mword_t) * 8);
	Nova::mword_t core_mask[num_vect];

	Genode::memset(core_mask, 0, sizeof(core_mask));

	_calculate_mask_for_location(core_mask, loc);

	log("Requested to create new cell for <", this->label(), "> of priority ", prioritiy, " at ", loc);
	for (unsigned i = 0; i < num_vect; i++) {
		if ((err = Nova::create_cell(_pd->pd_sel(), prioritiy, core_mask[i], i, 1) != Nova::NOVA_OK))
		{
			error("Could not create new cell: ", err);
		}
	}
}

void Pd_session_component::update_cell(const Affinity::Location &loc) 
{
	Nova::uint8_t err = Nova::NOVA_OK;
	unsigned num_cpus = platform_specific().affinity_space().total();
	unsigned num_vect = num_cpus / (sizeof(Nova::mword_t) * 8);
	Nova::mword_t core_mask[num_vect];

	_calculate_mask_for_location(core_mask, loc);

	for (unsigned i = 0; i < num_vect; i++) {
		if ((err = Nova::update_cell(_pd->pd_sel(), core_mask[i], i))) {
			error("Failed to update cell <", label(), ">: ", err);
		}
	}
}

using State = Genode::Pd_session::Managing_system_state;

State Pd_session_component::managing_system(State const &) { return State(); }

/*
 * \brief   Core-specific instance of the Cell interface
 * \author  Michael Müller
 * \date    2025-02-24
 */

/*
 * Copyright (C) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 *
 * This file is part of EalánOS based on the Genode OS framework, which are distributed
 * under the terms of the GNU Affero General Public Lisense version 3.
 */

#ifndef _CORE__INCLUDE__CELL_COMPONENT_H_
#define _CORE__INCLUDE__CELL_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/session_label.h>
#include <pd_session/client.h>
#include <util/list.h>
#include <platform_generic.h>

/* EalánOS includes */
#include <cell/cell.h>

/* Tukija includes */
#include <tukija/syscalls.h>
#include <tukija_native_pd/client.h>
#include <nova_util.h>

namespace Ealan {
    class Cell_component;
}

class Ealan::Cell_component : public Genode::Rpc_object<Cell>,
                              private Genode::List<Cell_component>::Element
{
    private:
        friend class Genode::List<Cell_component>;

        Genode::Rpc_entrypoint &_ep;
        Genode::Session_label const _session_label;
        Genode::Region_map &_rm;
        Genode::Pd_session_capability _pd_cap;
        Genode::Pd_session_client _pd;
        Genode::Tukija_native_pd_client _native_pd;

        Tukija::Cip *_cip{nullptr};

        bool _is_brick{false};
    
        void _calculate_mask_for_location(Tukija::Cpuset *coreset, const Genode::Affinity::Location &loc)
        {
            const_cast<Genode::Affinity::Location&>(loc).for_each(
                [&](Genode::Affinity::Location const &location)
                {
                    unsigned kernel_cpu = Core::platform_specific().kernel_cpu_id(location);
                    coreset->set(kernel_cpu);
                });
        }

        void _map_location_to_kernel(const Genode::Affinity &affinity)
        {
            unsigned local_idx = 0; /* Cell-local index */
            Genode::Affinity::Location const &loc = affinity.location();
            const_cast<Genode::Affinity::Location &>(loc).for_each(
                [&](Genode::Affinity::Location const &location)
                {
                    _cip->idx_to_phys_cpu_id[local_idx++] = Core::platform_specific().kernel_cpu_id(location);
                });
        }

        Cell_component(const Cell_component &);

        Cell_component& operator=(const Cell_component &);

    public:

        Cell_component(Genode::Pd_session_capability pd_cap, Genode::uint16_t prio, Genode::Affinity &affinity, Genode::Rpc_entrypoint &ep, Genode::Region_map &rm, Genode::Session_label const &label) : _ep(ep),  _session_label(label), _rm(rm), _pd_cap(pd_cap), _pd(pd_cap), _native_pd(_pd.native_pd()) {
            Tukija::mword_t cell_pd_sel = _native_pd.sel();
            Tukija::mword_t cip_phys = 0;

            /* Allocate a region map for mapping the CIP of this new cell. 
             * Only a region map needs to be allocated here, because the kernel will already
             * allocate a frame for this cell CIP during the syscall.
             */
            Core::platform().region_alloc().alloc_aligned(4 * Tukija::PAGE_SIZE_BYTE, Tukija::PAGE_SIZE_LOG2).with_result(
                [&](void *ptr) { _cip = static_cast<Tukija::Cip*>(ptr); },
                [&](Genode::Range_allocator::Alloc_error) { throw Genode::Out_of_ram(); });

            Tukija::mword_t cip_virt = reinterpret_cast<Tukija::mword_t>(_cip);

            /* Create cell at kernel. This will create a cell object in the kernel and allocate 
             * a page frame for the CIP. The CIP will then be mapped by the kernel using the 
             * supplied virtual address from the previously allocated region map.
             */
            if (Tukija::create_cell(cell_pd_sel, static_cast<Genode::uint8_t>(prio), cip_phys, cip_virt)) {
                Genode::error("Failed to create cell at Tukija.");
            }

            /* We need to specify the pre-reserved CPU cores from this cell. 
             * However, as the pre-resevred CPU cores are provided as Genode::Affinity
             * we need to convert it into a CPUset of the corresponding kernel cpu IDs.
             */
            _calculate_mask_for_location(&_cip->cores_reserved, affinity.location());
            Tukija::cell_ctrl(cell_pd_sel, Tukija::Cell_control::UPDATE_AFFINITY);
            
            Genode::log("Cores for <", label, ">: ", _cip->cores_reserved);

            /* Set affinity space this cell resides in */
            Genode::log("Affinity of cell ", label, ": ", affinity);
            _cip->habitat_affinity = affinity.space();
            _cip->location = affinity.location();

            /* As Genode operates on logical affinites, we need to set a mapping from Affinities
             * to kernel cpu IDs in order to make the user-space cell able to locate the correct
             * worker information structure for its worker threads.
             */
            _map_location_to_kernel(Genode::Affinity(affinity.space(), Genode::Affinity::Location(0,0,affinity.space().width(), affinity.space().height())));

            _ep.manage(this);
        }

        ~Cell_component()
        {
            _ep.dissolve(this);
        }

        /********************
         ** Cell interface **
         ********************/

        void update(Genode::Affinity &affinity) override {
            /* TODO: implement */
            Genode::log("Changing cell ", _session_label,"'s affinity to ", affinity);
            _cip->cores_reserved.clear();
            if (_cip->cores_reserved.count() != 0)
                Genode::error("Failed clearing reserved cores");
            _calculate_mask_for_location(&_cip->cores_reserved, affinity.location());
            Genode::log(_session_label, "'s cores: ", _cip->cores_reserved);
            Tukija::cell_ctrl(_native_pd.sel(), Tukija::Cell_control::UPDATE_AFFINITY);
        }

        bool is_brick() override {
            return _is_brick;
        }
};

#endif
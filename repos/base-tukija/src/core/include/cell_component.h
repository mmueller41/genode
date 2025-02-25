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
            for (unsigned y = loc.ypos(); y < loc.ypos() + loc.height(); y++)
            {
                for (unsigned x = loc.xpos(); x < loc.xpos()+loc.width(); x++)
                {
                    unsigned kernel_cpu = Core::platform_specific().kernel_cpu_id(Genode::Affinity::Location(x, y, loc.width(), loc.height()));
                    coreset->set(kernel_cpu);
                }
            }
        }

        Cell_component(const Cell_component &);

        Cell_component& operator=(const Cell_component &);

    public:

        Cell_component(Genode::Pd_session_capability pd_cap, Genode::uint16_t prio, Genode::Affinity &affinity, Genode::Rpc_entrypoint &ep, Genode::Region_map &rm, Genode::Session_label const &label) : _ep(ep),  _session_label(label), _rm(rm), _pd_cap(pd_cap), _pd(pd_cap), _native_pd(_pd.native_pd()) {
            Tukija::mword_t cell_pd_sel = _native_pd.sel();
            Tukija::mword_t cip_phys = 0;

            Core::platform().region_alloc().alloc_aligned(4 * Tukija::PAGE_SIZE_BYTE, Tukija::PAGE_SIZE_LOG2).with_result(
                [&](void *ptr) { _cip = static_cast<Tukija::Cip*>(ptr); },
                [&](Genode::Range_allocator::Alloc_error) { throw Genode::Out_of_ram(); });

            Tukija::mword_t cip_virt = reinterpret_cast<Tukija::mword_t>(_cip);

            if (Tukija::create_cell(cell_pd_sel, static_cast<Genode::uint8_t>(prio), cip_phys, cip_virt)) {
                Genode::error("Failed to create cell at Tukija.");
            }

            _calculate_mask_for_location(&_cip->cores_reserved, affinity.location());
            Genode::log("Cores for <", label, ">: ", _cip->cores_reserved);

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
            Genode::log("Changing cell's affinity to ", affinity);
            _calculate_mask_for_location(&_cip->cores_reserved, affinity.location());
            Tukija::cell_ctrl(_native_pd.sel(), Tukija::Cell_control::UPDATE_AFFINITY);
        }

        bool is_brick() override {
            return _is_brick;
        }
};

#endif
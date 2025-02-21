#ifndef _CORE__HABITAT_SESSION_COMPONENT_H_
#define _CORE__HABITAT_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <base/env.h>
#include <base/allocator.h>

#include <habitat/session.h>
#include <tukija/syscalls.h>
#include <tukija_native_pd/client.h>
#include <pd_session/client.h>

#include <platform.h>

#include <nova_util.h>

namespace Core { class Habitat_session_component; }

class Core::Habitat_session_component : public Genode::Rpc_object<Ealan::Habitat_session, Habitat_session_component>
{
    private:
        Genode::Region_map &_local_rm;
        Genode::Affinity::Space const &_space;

    void _calculate_mask_for_location(Tukija::Cpuset *coreset, const Genode::Affinity::Location &loc)
    {
        for (unsigned y = loc.ypos(); y < loc.ypos() + loc.height(); y++)
        {
            for (unsigned x = loc.xpos(); x < loc.xpos()+loc.width(); x++)
            {
                unsigned kernel_cpu = platform_specific().kernel_cpu_id(Genode::Affinity::Location(x, y, loc.width(), loc.height()));
                coreset->set(kernel_cpu);
            }
        }
    }

    public:
        Habitat_session_component(Genode::Region_map &rm, Genode::Affinity::Space const &space) : _local_rm(rm), _space(space) {}

        void create_cell(Genode::Capability<Genode::Pd_session> pd_cap, Genode::Affinity &affinity, Genode::uint16_t prio) override {

            Genode::Pd_session_client pd(pd_cap);

            Genode::Tukija_native_pd_client cell_pd_client(pd.native_pd());
            Genode::log("Attempting to create cell object for ", pd_cap);
            Tukija::mword_t cell_pd_sel = cell_pd_client.sel();

            Tukija::mword_t cip_phys = 0;
            
            Tukija::mword_t cip_virt = 0;
            platform().region_alloc().alloc_aligned(2 * Tukija::PAGE_SIZE_BYTE, Tukija::PAGE_SIZE_LOG2).with_result([&](void *ptr)
                                                                                                                          { cip_virt = reinterpret_cast<Tukija::mword_t>(ptr); },
                                                                                                                          [&](Genode::Range_allocator::Alloc_error) {});

            if (Tukija::create_cell(cell_pd_sel, static_cast<Genode::uint8_t>(prio), cip_phys, cip_virt))
            {
                Genode::error("Failed to create cell");
            }

            Genode::log("Mapped CIP from ", reinterpret_cast<void *>(cip_phys), " to ", cip_virt);
            unsigned long *cip = reinterpret_cast<unsigned long *>(cip_virt);
            Genode::log("CIP ", *cip);
            *cip = 0xff;

            _calculate_mask_for_location(coreset, affinity.location());


        }
};

#endif
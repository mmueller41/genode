/*
 * \brief   Topology service root component
 * \author  Michael Müller
 * \date    2022-10-06
 *
 * A topology session stores the component's view on the hardware topology, i.e. it's location within the NUMA topology.
 */

/*
 * Copyright (C) 2022 Michael Müller
 *
 * This file is part of EalánOS which is based on the Genode OS framework
 * released under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <root/component.h>

#include <topo_session_component.h>

#include <base/log.h>

namespace Genode {

    class Topo_root : public Root_component<Topo_session_component>
    {
        private:
            Ram_allocator   &_ram_alloc;
            Region_map &_local_rm;
            
        protected:

            Topo_session_component *_create_session(char const *args, Affinity const &affinity) override {
                size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

                if (ram_quota < Trace::Control_area::SIZE)
                    throw Insufficient_ram_quota();

                if (!affinity.valid()) {
                    log("Location ", affinity.location(), " not within space ", affinity.space());
                    throw Service_denied();
                }

                return new (md_alloc())
                    Topo_session_component(*this->ep(),
                                           session_resources_from_args(args),
                                           session_label_from_args(args),
                                           session_diag_from_args(args),
                                           _ram_alloc, _local_rm,
                                           const_cast<Affinity&>(affinity));
            }

            void _upgrade_session(Topo_session_component *topo, const char *args) override
            {
                topo->upgrade(ram_quota_from_args(args));
                topo->upgrade(cap_quota_from_args(args));
            }
        
        public:

            Topo_root(Ram_allocator     &ram_alloc,
                      Region_map        &local_rm,
                      Rpc_entrypoint    &session_ep,
                      Allocator         &md_alloc)
            :
                Root_component<Topo_session_component>(&session_ep, &md_alloc),
                _ram_alloc(ram_alloc), _local_rm(local_rm)
            { }
    };
}
#include <sandbox/child.h>
#include <sandbox/alias.h>
#include <sandbox/server.h>
#include <sandbox/heartbeat.h>
#include <sandbox/config_model.h>
#include <sandbox/library.h>
#include <sandbox/sandbox.h>

#include <base/log.h>
#include <base/registry.h>
#include <base/service.h>
#include <base/heap.h>

#include <util/reconstructible.h>

/* Hoitaja includes */
#include <core_allocator.h>
#include <cell.h>
#include <state_handler.h>

#pragma once
namespace Hoitaja {
    class Habitat;
    using namespace Genode;
}
struct Hoitaja::Habitat : public Sandbox::Library
{
    public:

        friend class Genode::Sandbox::Local_service_base;

        State_handler &_habitat_handler;

        Heap _heap;
        Sliced_heap suoritin_heap;


        Genode::Constructible<Hoitaja::Core_allocator> _core_allocator;


        Registry<Genode::Sandbox::Local_service_base>
            _local_services{};

        void apply_config(Xml_node const &config) override {
            log("Hoitaja is applying new config.");

            Sandbox::Library::apply_config(config);
        }

        void generate_state_report(Xml_generator &xml) const override {
            log("Generating new state report for Hoitaja.");
            Sandbox::Library::generate_state_report(xml);
        }

        void maintain_cells();

        /**
         * @brief Update cell's resource allocations
         * 
         * @param cell whose resource allocations needs updating
         */
        void update(Cell &cell);

        Habitat(Env &env, State_handler &habitat_handler, Genode::Sandbox::State_handler &handler)
            : Sandbox::Library(env, _heap, _local_services, handler), _habitat_handler(habitat_handler), _heap(env.ram(), env.rm()),
            suoritin_heap(env.ram(), env.rm()), _core_allocator()
        {
        }

        Sandbox::Child &create_child(Xml_node const &) override;

        void _destroy_abandoned_children() override;
};
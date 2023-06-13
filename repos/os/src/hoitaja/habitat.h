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

namespace Hoitaja {
    class Habitat;
    using namespace Genode;
}
class Hoitaja::Habitat : Sandbox::Library
{
    
    private:
        friend class Genode::Sandbox::Local_service_base;

        Heap _heap;

        Registry<Genode::Sandbox::Local_service_base>
            _local_services{};

    public:
        void apply_config(Xml_node const &config) override {
            log("Hoitaja is applying new config.");
            Sandbox::Library::apply_config(config);
        }

        void generate_state_report(Xml_generator &xml) const override {
            log("Generating new state report for Hoitaja.");
            Sandbox::Library::generate_state_report(xml);
        }

        void maintain_cells();

        Habitat(Env &env, Genode::Sandbox::State_handler &handler)
            : Sandbox::Library(env, _heap, _local_services, handler), _heap(env.ram(), env.rm())
        {
        }
};
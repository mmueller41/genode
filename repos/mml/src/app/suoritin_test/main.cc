#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/thread.h>
#include <base/attached_dataspace.h>
#include <base/attached_rom_dataspace.h>

#include <suoritin/connection.h>
#include <timer_session/connection.h>
#include <trace/timestamp.h>

#include <nova/syscall-generic.h>
#include <nova/syscalls.h>
class Worker : public Genode::Thread {

    public:
        Worker(Genode::Env &env)
            : Genode::Thread(env, Name("worker foo"), 4 * 4096) { }

        void entry() override
        {
            while(true) {

            }
        }
};

struct Suoritin_tester
{
    Genode::Env &_env;
    Genode::Heap _heap{_env.ram(), _env.rm()};

    Genode::Attached_rom_dataspace _config{_env, "config"};

    Suoritin_tester(Genode::Env &env) : _env(env)
    {
        bool greedy = false; // Shall this cell always request more resources? default: no
        //Genode::log("Hello from Suoritin tester");
        Tukija::Suoritin::Connection suoritin {_env};
        //Genode::log("Created TASKING session");

        //Genode::log("Reading config");
        _config.update();

        if (_config.valid()) {
            greedy = _config.xml().attribute_value("greedy", false);
        }

        /* Create a single dummy worker */
        Worker *foo = new (_heap) Worker(_env);
        Worker *bar = new (_heap) Worker(_env);

        //Genode::log("Querying dataspace capabilities for shared mem interface");
        Genode::Dataspace_capability workers_cap = suoritin.worker_if();
        Genode::Dataspace_capability channels_cap = suoritin.channel_if();
        Genode::Dataspace_capability evt_cap = suoritin.event_channel();

        //Genode::log("Mapping interface into virtual address space");
        Tukija::Suoritin::Worker* workers = env.rm().attach(workers_cap);
        Tukija::Suoritin::Channel *channels = env.rm().attach(channels_cap);
        //Tukija::Suoritin::Event_channel *evtchn = env.rm().attach(evt_cap);

        //Genode::log("Registering dummy worker named foo");
        suoritin.register_worker(Genode::Thread::Name("foo"), foo->cap());

        //Genode::log("Creating one dummy task queue aka channel ");
        suoritin.create_channel(*workers);

        suoritin.register_worker(Genode::Thread::Name("bar"), bar->cap());
        //Genode::log("Interfaces mapped succesfully");
        //Genode::log("Workers interface: ", workers);
        //Genode::log("Channels interface: ", channels);

        //Genode::log("Writing dummy values");
        if (greedy)
            channels[0].length(0xDEADBEEF);
        else
            channels[0].length(0x20);
        // //Genode::log("Channel 0 has length ", channels[0].length());

        suoritin.create_channel(workers[1]);
        channels[1].length(0xF00);

       // Genode::log("Waiting for parent to react");

        Genode::Trace::Timestamp rpc_delay = 0;

        if (greedy) {
            Nova::mword_t response = 0;
            Genode::Trace::Timestamp start = Genode::Trace::timestamp();

            for (unsigned long i = 0; i < 2000; i++) {
                channels[0].length(0xDEADBEEF);
                
               // Genode::Trace::Timestamp start = Genode::Trace::timestamp();
                //Nova::hpc_read(0, 1, response);
                suoritin.request_cores();
                // Genode::Trace::Timestamp end = Genode::Trace::timestamp();
                // rpc_delay += (end - start);

                /*while (!(__atomic_load_n(&evtchn->parent_flag, __ATOMIC_SEQ_CST)))
                    __builtin_ia32_pause();
                __atomic_store_n(&evtchn->parent_flag, false, __ATOMIC_SEQ_CST);*/
            }
            Genode::Trace::Timestamp end = Genode::Trace::timestamp();
            Genode::log("{\"response\": \"", response, "\", \"delay\": ", (end-start)/4000, ", \"rpc\":", rpc_delay/4000, "},");
        }

        //Genode::log("Exiting");
        while(true)
            ;
        //_env.parent().exit(0);
    }
};

void Component::construct(Genode::Env &env) {
    try {
        static Suoritin_tester tester(env);
    } catch (Genode::Quota_guard<Genode::Cap_quota>::Limit_exceeded) {
        Genode::error("Failed: Caps exceeded.");
    }
}
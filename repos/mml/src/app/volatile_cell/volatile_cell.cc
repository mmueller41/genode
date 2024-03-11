#include <base/component.h>
#include <base/env.h>
#include <timer_session/connection.h>
#include <base/log.h>
#include <base/thread.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <nova/syscalls.h>
#include "loop.h"
namespace Hoitaja_test {
    class Volatile_cell;
    struct Worker;
}

#define LOOPS 100000
#define ALLOC

static Nova::mword_t channel;

void short_loop(unsigned long k)
{
    for (unsigned long i = 0; i < k; i++) {
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
    }
}

struct Hoitaja_test::Worker : public Genode::Thread
{
    Genode::uint16_t _id;

    void entry() override
    {
        Nova::mword_t channel_id = 0;
        Nova::cpu_id(channel_id);
        unsigned long volatile *my_channel = &reinterpret_cast<unsigned long volatile *>(channel)[channel_id];

        //Genode::log("Started worker ", _id, " on CPU ", channel_id);
        Nova::yield();
        while (true) {
            while (!(__atomic_load_n(my_channel, __ATOMIC_SEQ_CST)))
                short_loop(770);
            //Genode::log("Returning core ", channel_id);
            Nova::yield(false);
        }
    }

    Worker(Genode::Env &env, Genode::uint16_t id, Location const &location)
        : Thread(env, Name("test_", location.xpos(), "x", location.ypos()), 4 * 4096, location, Weight(), env.cpu()), _id(id)
    { }
};

class Hoitaja_test::Volatile_cell
{
    private:
        Genode::Env &_env;
        Timer::Connection _timer{_env};

    public:
        Volatile_cell(Genode::Env &env) : _env(env) 
        {
            Genode::log("My affinity space is ", _env.cpu().affinity_space());

            /* Pseuda-MxTasking Initialization */
            Nova::uint8_t rc = 0;
            Genode::Ram_dataspace_capability ds = env.ram().alloc(4096);
            channel = env.rm().attach(ds);

            if ((rc = Nova::mxinit(0, 0, channel))) {
                Genode::error("Failed to init MxTasking: ", rc);
            }

            Genode::Heap _heap{env.ram(), env.rm()};

            Nova::mword_t my_cpu = 0;
            Nova::cpu_id(my_cpu);

            Genode::log("Main thread on CPU ", my_cpu);

            unsigned cores_total = env.topo().global_affinity_space().total();


            for (Genode::uint16_t cpu = 1; cpu < cores_total; cpu++)
            {
                if (cpu == cores_total - my_cpu)
                    continue;
                //Genode::log("Created worker for CPU ", cpu);
                Worker *worker = new (_heap) Worker(env, cpu, env.topo().global_affinity_space().location_of_index(cpu));
                worker->start();
            }
            Nova::mword_t mask = 0;
            Nova::core_allocation(mask, true);

            _timer.msleep(my_cpu);

            Genode::log("Initial allocation: ", mask);

            for (; ;) {
#ifdef ALLOC
                Nova::mword_t allocation = 0;
                if (Nova::alloc_cores(32, allocation) != Nova::NOVA_OK) {
                    //Genode::error("Failed to allocate cores");
                }
                /*if (allocation != mask)
                {
                    Genode::log("Allocated cmap = ",allocation);
                }*/
                //_timer.usleep(2000);
                while ((allocation != (mask | 0x1)) && (allocation != mask) ){
                    Nova::core_allocation(allocation);
                    //Genode::log("Core allocation = ", allocation, " mask = ", mask);
                    __builtin_ia32_pause();
                }

                loop(500);
#endif

                //_timer.msleep(20);
            }
            while (true)
                ;
            // Genode::log("My time has come. Exiting ... cmap = ", allocation);
            _timer.msleep(1000UL * 1000);
            _env.parent().exit(0);
        }
};

void Component::construct(Genode::Env &env) { static Hoitaja_test::Volatile_cell cell(env); }
#include <libc/component.h>
#include <base/log.h>
#include <nova/syscall-generic.h>
#include <nova/syscalls.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <cstdio>
#include <iostream>
#include <internal/thread_create.h>
#include <thread>
#include <atomic>
#include <timer_session/connection.h>

#define CALLS 100
#define CORES 4
#define HYPERCALL

    //Genode::Trace::timestamp();
static Genode::Trace::Timestamp rdtsc_cost = 0;
Genode::Env *genv = nullptr;
static Genode::Trace::Timestamp start = 0;
static const unsigned long loops = 10000UL;
static Nova::mword_t channel = 0;
static std::atomic<long> counter(0);
static std::atomic<bool> ready{false};
static std::atomic<bool> restart{true};
static std::atomic<int> yield_ctr{-(31-CORES)};
static unsigned long tsc_freq_khz = 0;

struct Cell : public Genode::Thread
{
    Genode::uint16_t _id;
    Libc::Env &env;
    Timer::Connection &_timer;

    static void *pthread_entry(void *args) {
        Cell *cell = reinterpret_cast<Cell *>(args);
        cell->entry();
        return nullptr;
    }

    void entry() override
    {
        Nova::mword_t channel_id = 0;
        Nova::uint64_t count_allocs = 0;
        Nova::cpu_id(channel_id);
        unsigned long volatile *my_channel = &reinterpret_cast<unsigned long volatile *>(channel)[channel_id];

        unsigned long _tsc_freq_ghz = tsc_freq_khz / 1000000UL;

        Genode::log("Started worker", _id, " on CPU with affinity ", channel_id, Genode::Thread::myself()->affinity(), " signal channel: ", *my_channel, " at ", my_channel);

        for (int cores = CORES; cores < 32; cores+=4) {
            for (int i = 0; i < CALLS; ) {

                if ((i == 0 && yield_ctr >= cores-1) || (i > 0 && yield_ctr >= cores-1))
                    ready = true;

                if (_id != 0 && restart.load()) {
                    yield_ctr.fetch_add(1);
                    // Genode::log("Worker ", _id, "yielded, yield_ctr = ", yield_ctr.load());
                    Nova::yield();
                }

                //Genode::log("Worker ", _id, " on CPU ", channel_id, " woke up");
                counter.fetch_add(1);
                    if (counter >= cores-1) {
                        ready = true;
                        // Genode::log("{\"allocation:\": ", allocation, ", \"id\":", _id, ",\"clk_total\":", (end-::start), ", \"mean_clk\":", (end-::start)/count_allocs ,", \"count\": ", count_allocs, "\"channel-id\":", channel_id, "},");
                    }

                if (_id == 0) {
                    //Genode::log("Waiting on start signal");
                    while (ready.load() == false)
                        __builtin_ia32_pause();

                    //Genode::log("Got start signal");
                    _timer.msleep(20);

                    //Genode::log("Woke up for new iteration");
                    ready = false;
                    restart = false;
                    ::start = Genode::Trace::timestamp();
                }

                Genode::Trace::Timestamp end = 0;
                while (_id==0)
                {

                    if (_id == 0) {
                        Nova::mword_t allocated = 0;
                        //Genode::log("Allocating 4 cores");
                        
                        Nova::uint8_t rc = Nova::alloc_cores(cores, allocated);
                        if (rc == Nova::NOVA_OK)
                        {

                            while(ready.load() == false)
                                __builtin_ia32_pause();
                            end = Genode::Trace::timestamp();
                            Nova::mword_t allocation = 0;
                            Nova::core_allocation(allocation);
                            restart = true;
                            i++;
                            counter = 0;
                            yield_ctr = 0;
                            Genode::log("{\"cores\":", cores, "\"allocation\": ", allocation, ",\"start\": ", ::start, ", \"end\": ", end, " ,\"us\": ", (end - ::start)/_tsc_freq_ghz,"},");
                            break;
                        } else {
                            //Genode::log("cores allocated: ", allocated);
                            break;
                            // Genode::log("cores allocated: ", allocated);
                        }
                        count_allocs++;
                    }
                }
                //Genode::log("Finished allocation. Waiting for yield signal, id = ", channel_id, "\n");
                while (restart.load() == false) {
                    long res = 0;
                    if ((res = __atomic_load_n(my_channel, __ATOMIC_SEQ_CST)) != 0)
                    {
                        Genode::log("Got yield signal on channel ", channel_id, " signal: ", res, " at ", my_channel);
                        Nova::yield(false);
                    }
                }
            }
        }
        Genode::log("Benchmak finished.");
    }
    Cell(Libc::Env &env, Timer::Connection &timer, Genode::uint16_t id, Location const &location)
        : Thread(env, Name("test_", location.xpos(), "x", location.ypos()), 4 * 4096, location, Weight(), env.cpu()), _id(id), env(env), _timer(timer)
    { }
};


void Libc::Component::construct(Libc::Env &env)
{
    Nova::uint8_t res = 0;
    genv = &env;

    Libc::with_libc([&]()
                    {
    Timer::Connection _timer{env};

    Genode::Ram_dataspace_capability ds = env.ram().alloc(4096);
    channel = env.rm().attach(ds);

    Genode::memset(reinterpret_cast<char *>(channel), 0, 4096);

    //Genode::Heap _heap{env.ram(), env.rm()};

    //Genode::log("Registering MxTasking entrypoint");
    if ((res = Nova::mxinit(0, 0, channel))) {
        Genode::error("Failed to init MxTasking: ", res);
    }
    Genode::log("Registered MxTasking, yielding ...");
    
    try {
        Genode::Attached_rom_dataspace info(env, "platform_info");
        tsc_freq_khz = info.xml().sub_node("hardware").sub_node("tsc")
                        .attribute_value("freq_khz", 0ULL);
    } catch (...) { };
 
    start = Genode::Trace::timestamp();
    for (unsigned c = 0; c < 1000; c++) {
        //Genode::Trace::Timestamp start = Genode::Trace::timestamp();
        
        /*Nova::uint8_t rc = Nova::yield();
        if (rc != Nova::NOVA_OK)
            break;*/
        Genode::Trace::timestamp();
        // Genode::Trace::Timestamp end = Genode::Trace::timestamp();
        // delay += (end - start);
    }
    Genode::Trace::Timestamp end = Genode::Trace::timestamp();
    rdtsc_cost = (end - start) / 1000 / 2;

    Genode::log("My affinity is ", env.cpu().affinity_space(), " of size ", env.cpu().affinity_space().total());
    Genode::log("Will create workers for affinity space: ", env.topo().global_affinity_space());
    start = Genode::Trace::timestamp();
    Genode::Thread *me = Genode::Thread::myself();

    unsigned long cpuid = 0;
    Nova::cpu_id(cpuid);

    Genode::Affinity::Space space = env.topo().global_affinity_space();
    Genode::log("My main thread is on phys. CPU ", cpuid);

    pthread_t workers[space.total()];
    std::cout << "Creating workers" << std::endl;
    for (Genode::uint16_t cpu = 1; cpu < space.total(); cpu++)
    {
        Genode::String<32> const name{"worker", cpu};
        if (cpu == (space.total() - cpuid))
            continue;
        Cell *worker = new Cell(env, _timer, cpu, space.location_of_index(cpu));
        Libc::pthread_create_from_session(&workers[cpu], Cell::pthread_entry, worker, 4 * 4096, name.string(), &env.cpu(), space.location_of_index(cpu));
        // Genode::log("Created worker for CPU ", cpu);
        // worker->start();
    }

    pthread_t main_pt{};

    Genode::Affinity::Location loc = me->affinity();
    //Genode::log("Starting main worker on CPU ", cpuid);
    Cell *main_cell = new Cell(env, _timer, 0, loc);

    //Cell *main = new (_heap) Cell(env, 0, Genode::Affinity::Location(20,0));
    /*Libc::pthread_create_from_thread(&main_pt, *main, &main);
    main->start();*/
    // Nova::yield(false);
    _timer.msleep(150);
    Libc::pthread_create_from_session(&main_pt, Cell::pthread_entry, main_cell, 8 * 4096, "main_worker", &env.cpu(), loc);
    pthread_join(main_pt, 0); });
    Genode::log("Leaving component");
}
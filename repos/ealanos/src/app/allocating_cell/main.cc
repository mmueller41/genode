#include <libc/component.h>
#include <base/log.h>
#include <tukija/syscall-generic.h>
#include <tukija/syscalls.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <cstdio>
#include <iostream>
#include <internal/thread_create.h>
#include <thread>
#include <atomic>
#include <timer_session/connection.h>

#define CALLS 100
#define CORES 14
#define HYPERCALL

    //Genode::Trace::timestamp();
//static Genode::Trace::Timestamp rdtsc_cost = 0;
Genode::Env *genv = nullptr;
static Genode::Trace::Timestamp start = 0;
static const unsigned long loops = 10000UL;
static std::atomic<long> counter(0);
static std::atomic<bool> ready{false};
static std::atomic<bool> restart{true};
static std::atomic<int> yield_ctr{-(31-CORES)};
static unsigned long tsc_freq_khz = 0;
static Tukija::Cip *cip = Tukija::Cip::cip();
static Tukija::Tip const *tip = Tukija::Tip::tip();

int cores, i;


struct Channel {
    unsigned long yield_flag : 1,
        op : 2,
        tnum : 61;
    unsigned long delta_alloc;
    unsigned long delta_activate;
    unsigned long delta_setflag;
    unsigned long delta_findborrower;
    unsigned long delta_block;
    unsigned long delta_enter;
    unsigned long delta_return;
};

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
        Genode::Trace::Timestamp latency = 0;
        Tukija::uint64_t count_allocs = 0;

        Tukija::Cip::Worker *my_channel = &cip->worker_for_location(Genode::Thread::myself()->affinity());
        unsigned channel_id = cip->location_to_kernel_cpu(Genode::Thread::myself()->affinity());

        unsigned long _tsc_freq_ghz = tsc_freq_khz / 1000000UL;

        Genode::log("Started worker", _id, " on CPU with affinity ", channel_id, "@", const_cast<Tukija::Tip*>(tip)->dom_of_cpu(channel_id).id, "-", Genode::Thread::myself()->affinity(), " signal channel: ", my_channel->yield_flag, " at ", my_channel);

        for (cores = CORES; cores <= 14; cores+=4) {
            for (i = 0; i < CALLS; ) {

                if ((i == 0 && yield_ctr >= cores-1) || (i > 0 && yield_ctr >= cores-1))
                    ready = true;

                if (_id != 0 && restart.load()) {
                    yield_ctr.fetch_add(1);
                    // Genode::log("Worker ", _id, "yielded, yield_ctr = ", yield_ctr.load());
                    Tukija::release(Tukija::Resource_type::CPU_CORE);
                }

                //Genode::log("Worker ", _id, " on CPU ", channel_id, " woke up");
                counter.fetch_add(1);
                if (counter >= cores-1) {
                    ready = true;
                    // Genode::log("{\"allocation:\": ", allocation, ", \"id\":", _id, ",\"clk_total\":", (end-::start), ", \"mean_clk\":", (end-::start)/count_allocs ,", \"count\": ", count_allocs, "\"channel-id\":", channel_id, "},");
                }

                /*if (my_channel->op == 2) {
                    Genode::Trace::Timestamp now = Genode::Trace::timestamp();
                    //Tukija::core_allocation(allocation);
                    my_channel->delta_return = now - my_channel->delta_return;
                    Genode::log("{\"iteration\": ", i, ", \"cores\":", cores, ", \"d_block\": ", my_channel->delta_block / _tsc_freq_ghz, ", \"d_enter\":", my_channel->delta_enter / _tsc_freq_ghz, ", \"d_return\":", my_channel->delta_return / _tsc_freq_ghz, ", \"op\": \"yield\"},");
                }
                my_channel->op = 0;*/
                if (_id == 0) {
                    //Genode::log("Waiting on start signal");
                    while (ready.load() == false)
                        __builtin_ia32_pause();

                    //Genode::log("Got start signal");
                    _timer.msleep(2);

                    //Genode::log("Woke up for new iteration");
                    ready = false;
                    restart = false;
                    ::start = Genode::Trace::timestamp();
                }

                Genode::Trace::Timestamp end = 0;
                while (_id==0)
                {

                    if (_id == 0) {
                        //Genode::log("Allocating 4 cores");

                        //my_channel->tnum = i;
                        //my_channel->op = 1; /* 1 for alloc, 2 for yield */

                        //my_channel->delta_enter = Genode::Trace::timestamp();
                        Tukija::uint8_t rc = Tukija::alloc(Tukija::Resource_type::CPU_CORE, cores);
                        if (rc == Tukija::NOVA_OK)
                        {

                            while(ready.load() == false)
                                __builtin_ia32_pause();
                            end = Genode::Trace::timestamp();
                            //my_channel->delta_return = end - my_channel->delta_return;
                            latency += (end - ::start) / _tsc_freq_ghz;
                            //Genode::log("{\"iteration\": ", i, ", \"cores\":", cores, ", \"delta_enter:\" ", my_channel->delta_enter / _tsc_freq_ghz, ", \"delta_alloc\": ", my_channel->delta_alloc / _tsc_freq_ghz, ", \"delta_activate:\": ", my_channel->delta_activate / _tsc_freq_ghz, ", \"delta_setflag\": ", my_channel->delta_setflag / _tsc_freq_ghz, ", \"delta_return\": ", my_channel->delta_return / _tsc_freq_ghz, "},");
                            restart = true;
                            counter = 0;
                            yield_ctr = 0;
                            //if (i%100==0) {

                            Genode::log("{\"iteration\": ", i, ", \"cores\":", cores, ", \"allocation\": ", cip->cores_new, ",\"start\": ", ::start, ", \"end\": ", end, " ,\"ns\": ", (latency), "},");
                            //my_channel->delta_setflag = 0;
                            latency = 0;
                            cip->cores_new.clear();
                            //}
                            i++;
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
                    Tukija::Cip::Worker volatile *res = __atomic_load_n(&my_channel, __ATOMIC_SEQ_CST);
                    if (res->yield_flag) {
                        //Genode::log("Got yield signal on channel ", channel_id);
                        Tukija::return_to_owner(Tukija::Resource_type::CPU_CORE);
                    }
                }
            }
        }
        Genode::log("Benchmak finished.");
    }
    Cell(Libc::Env &env, Timer::Connection &timer, Genode::uint16_t id, Location const &location)
        : Thread(env, Name("test_", location.xpos(), "x", location.ypos()), 4 * 4096, location, Weight(), 
        env.cpu()), _id(id), env(env), _timer(timer)
    { }
};


void Libc::Component::construct(Libc::Env &env)
{
    genv = &env;

    Libc::with_libc(
        [&]() {
            Timer::Connection _timer{env};

            Genode::Heap _heap{env.ram(), env.rm()};

            Genode::log("Registered MxTasking, yielding ...");
    
            try {
                Genode::Attached_rom_dataspace info(env, "platform_info");
                tsc_freq_khz = info.xml().sub_node("hardware").sub_node("tsc")
                                .attribute_value("freq_khz", 0ULL);
            } catch (...) { };
            
            Genode::log("My affinity is ", env.cpu().affinity_space(), " of size ", 
                env.cpu().affinity_space().total());

            Genode::log("Running at a frequency of ", tsc_freq_khz, " KHz");

            start = Genode::Trace::timestamp();
            Genode::Thread *me = Genode::Thread::myself();

            Genode::Affinity::Space space = cip->habitat_affinity;

            pthread_t workers[space.total()];
            std::cout << "Creating workers" << std::endl;
            Genode::Trace::Timestamp thread_start = Genode::Trace::timestamp();
            
            Genode::Affinity::Location loc = me->affinity();
            unsigned cpuid = cip->location_to_kernel_cpu(loc);

            for (Genode::uint16_t cpu = 1; cpu < space.total(); cpu++)
            {
                Genode::String<32> const name{"worker", cpu};
                /* Do not create a worker for EP's CPU yet, because
                   we want to measure the time it takes to create only the worker threads */
                if (cpu == (space.total() - cpuid))
                    continue;

                Cell *worker = new Cell(env, _timer, cpu, space.location_of_index(cpu));
                Libc::pthread_create_from_session(&workers[cpu], Cell::pthread_entry, 
                    worker, 4 * 4096, name.string(), &env.cpu(), space.location_of_index(cpu));
            }

            Genode::Trace::Timestamp thread_stop = Genode::Trace::timestamp();
            Genode::log("Took ", (thread_stop - thread_start) / (tsc_freq_khz/1000), " μs to start workers");

            pthread_t main_pt{};

            Genode::log("Starting main worker on CPU ", cip->location_to_kernel_cpu(loc));
            Cell *main_cell = new Cell(env, _timer, 0, loc);

            Libc::pthread_create_from_session(&main_pt, Cell::pthread_entry, 
                main_cell, 8 * 4096, "main_worker", &env.cpu(), loc);
            pthread_join(main_pt, 0); 
        });
    Genode::log("Leaving component");
}
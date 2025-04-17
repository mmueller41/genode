/**
 * @file main.cc
 * @author Michael Müller <michael.mueller@uos.de>
 * @brief Micro-benchmark for evaluating CPU core allocation costs
 * @details This micro-benchmark measures the time for allocating and activating a certain number of CPU cores.
 * @version 0.1
 * @date 2025-03-10
 * 
 * @copyright Copyright (c) 2025
 * 
 */
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
#include <vector>

int CALLS = 0; /* Number of times to repeat the allocation of CPU cores */
int CORES{0};  /* Initial number of CPUs to allocate */
int STEP{1};  /* How many CPU cores to add after each CALLS iterations. */

/* Global parameters */
Genode::Env *genv = nullptr;
static Genode::Trace::Timestamp start = 0; /* Start point in time */
static const unsigned long loops = 10000UL; /* Number of times an allocation shall be repeated, needed to get
 a statistically meaningful number of data points */
static std::atomic<long> counter(0); /* Atomic counter incremened by activated workers, used as barrier */
static bool volatile  ready{false}; /* Signal flag that all workers are set up and ready for benchmarking */
static bool volatile restart{true}; /* Signals that the benchmark run shall restart */
static bool volatile quit{false};
static std::atomic<int> yield_ctr{-(63 - CORES)}; /* Counter for the number of workers that must yield before
                                                   a new benchmark run can be started. */
static unsigned long tsc_freq_khz = 0;  /* TSC frequency in kHz, used for calculating measured time intervals */
static Tukija::Cip *cip = Tukija::Cip::cip(); /* Cell info page, stores info about the current core allocation */
static Tukija::Tip const *tip = Tukija::Tip::tip(); /* Used to query topology information */

static Tukija::Cpuset active_workers{0};

int cores, i; /* Global iterator variables */
static bool constexpr print_allocation{false};
struct Cell : public Genode::Thread
{
    Genode::uint16_t _id;
    Libc::Env &env;
    Timer::Connection &_timer;

    /**
     * @brief pthread wrapper function for a Genode thread's entry method.
     * 
     * @param args 
     * @return void* 
     */
    static void *pthread_entry(void *args) {
        Cell *cell = reinterpret_cast<Cell *>(args);
        cell->entry();
        return nullptr;
    }

    void worker_loop() {
        Tukija::Cip::Worker *my_channel = &cip->worker_for_location(Genode::Thread::myself()->affinity());
        unsigned channel_id = cip->location_to_kernel_cpu(Genode::Thread::myself()->affinity());
        
        //Genode::log("Started worker", _id, " on CPU with affinity ", channel_id, "@", const_cast<Tukija::Tip*>(tip)->dom_of_cpu(channel_id).id, "-", Genode::Thread::myself()->affinity(), " signal channel: ", my_channel->yield_flag, " at ", my_channel);
        
        while (true) {
            
            /* If the current thread is not the main worker, i.e its _id is not 0, (that allocates CPU cores), 
                it should sleep voluntarily releasing its CPU core. This ensures that 
                we can measure the allocation of CPU cores and the activation of workers 
                in one row. */
            if (__atomic_load_n(&restart, __ATOMIC_SEQ_CST)) {
                active_workers.clr(channel_id);
                // Genode::log("Worker ", _id, " on CPU ", channel_id, " yielding. cores=", cip->cores_current, " #:", cip->cores_current.count());
                Tukija::release(Tukija::Resource_type::CPU_CORE);
                active_workers.set(channel_id);
            }

            /* The thread was woken up, so increase the thread counter */

            if (counter.fetch_add(1) == cores-1) {
                __atomic_store_n(&ready, true, __ATOMIC_SEQ_CST);
                //Genode::log("Worker", _id, " Signaled ready, ctr = ", counter.load());
            }
            //Genode::log("Worker ", _id, " woke up on CPU ", channel_id, " counter=", counter.load());


            /* As long as no restart was signalled, poll the yield flag to check whether
               we received a yield request from the hypervisor. */
            if ((my_channel->yield_flag != 0))
            {
                //Genode::log("Worker returning CPU ", channel_id);
                active_workers.clr(channel_id);
                Tukija::return_to_owner(Tukija::Resource_type::CPU_CORE);
            }

            if (__atomic_load_n(&quit, __ATOMIC_SEQ_CST)) {
                break;
            }
            //Genode::log("Worker ", _id, " on CPU ", channel_id, " restarting.");
        }
    }

    /**
     * @brief the actual benchmark
     * 
     */
    void benchmark_loop()
    {
        Genode::Trace::Timestamp latency = 0;

        /* First determine our channel id and store the pointer to our 
           CIP worker structure. This is needed in order to get the hypervisor's yield signal. */

        /* Calculate TSC frequency in GHz */
        unsigned long _tsc_freq_ghz = tsc_freq_khz / 1000000UL;

        int max_cores = CORES;
         //cip->habitat_affinity.total() - 1; /* Allocate up to the number of cores in habitat except for the one that runs our benchmark */

        /* Now allocate cores starting with a number of CORES and increase by 4 additional cores 
           after CALLS iterations. */
        for (; cores <= max_cores;)
        {
            Genode::log("Starting benchmark for ", cores, " CPU cores. Parameters: ", yield_ctr.load(), ":", counter.load());
            for (i = 0; i < CALLS;)
            {
                int loops = 1;

                /* Here, we check, if the yield counter is equally to the number
                    of cores to allocate-1. This ensures that all workers have gone to sleep
                    and released their CPU core. */
                //Genode::log("Waiting for ", cip->cores_current.count(), " workers to yield");
                while (cip->cores_current.count() != 1 )
                {
                    __builtin_ia32_pause();
                    /*
                    if ((loops % 10000) == 0)
                        Genode::log("Waiting for all to yield.", cip->cores_current, " active: ", active_workers);
                    */
                    loops++;
                }

                //Genode::log("Workers ready.");
                //_timer.msleep(10);

                restart = false;
                ready = false;
                /* Mark beginning of benchmark */
                ::start = Genode::Trace::timestamp();
                
                Genode::Trace::Timestamp end = 0;
                /* Allocated the specified number of CPU cores */
                Tukija::uint8_t rc = Tukija::alloc(Tukija::Resource_type::CPU_CORE, cores);
                if (rc == Tukija::NOVA_OK)
                {
                    unsigned cores_allocated = cip->cores_new.count() + cip->cores_reclaimed.count();
                    unsigned cores_reclaimed = cip->cores_reclaimed.count();

                    if (cores_allocated < static_cast<unsigned>(cores)) {
                        //Genode::log("Cores activated ", cip->cores_current.count());
                        //Genode::log("Reached maximum limit early at ", cores_allocated, " cores new: ", cip->cores_new);
                       //Genode::log("Cores reclaimced: ", cip->cores_reclaimed);
                    }

                    //Genode::log("Cores allocated: ", cip->cores_new);
                    //Genode::log("Cores activated ", cip->cores_current);
                    /* If we get NOVA_OK as return code, cores were allocated. However, it is not guaranteed yet
                       that we got the specified number of CPU cores (it could be less). So, we have to wait until
                       all requested workers have actually woken up and see if the number of waken threads matches
                       the requested number of CPU cores. */
                    //Genode::log("Waiting for workers to show up");
                    //loops = 1;

                    while (counter.load() < cores_allocated)
                    {
                        __builtin_ia32_pause();
                        /*if ((loops % 10000) == 0) {
                            Genode::log("Waiting for ", cores_allocated, " cores. Counter =", counter.load(), " cores ", cip->cores_current, " cores awaited: ", cip->cores_reclaimed, cip->cores_new);
                        }
                        loops++;*/
                    }
                    //Genode::log("All CPUs received.");

                    /* Now, we need to restart the run. Hence, we set the restart flag to true and 
                       reset the counter variables. */
                    counter = 0;
                    yield_ctr.store(0);
                    ready = false;

                    /* ALl requested CPU cores have been allocated and workers were activated. So we can now
                       mark the end of this run, and calculate the time this run took. */
                    end = Genode::Trace::timestamp();
                    latency += (end - ::start) / _tsc_freq_ghz;

                    /* Print the results out in JSON */
                    if constexpr (print_allocation)
                        Genode::log("{\"iteration\": ", i, ", \"cores\":", cores_allocated, ",\"allocation\": ", active_workers, ",\"start\": ", ::start, ", \"end\": ", end, " ,\"ns\": ", (latency), "},");
                    else 
                        Genode::log("{\"iteration\": ", i, ", \"cores\":", cores_allocated, ", \"cores_wanted\":", cores, ", \"reclaimed\":", cores_reclaimed,  ",\"reserved\": ", cip->cores_reserved.count(), ",\"ns\": ", (latency), "},");
                    latency = 0;

                    //Genode::log("Restarting.");
                    /* Also clear the CPUset of new cores, so that we will not see cores allocated by previous runs. */
                    cip->cores_new.clear();
                    cip->cores_reclaimed.clear();
                    if (++i == CALLS) {
                        cores = (cores == max_cores-(max_cores%STEP)) ? max_cores : cores + STEP;
                        if (cores >= max_cores) {
                            Genode::log("Benchmark finished.");
                            cores = max_cores + 1;
                            __atomic_store_n(&quit, true, __ATOMIC_SEQ_CST);
                        } else
                            __atomic_store_n(&restart, true, __ATOMIC_SEQ_CST);

                    } else
                        __atomic_store_n(&restart, true, __ATOMIC_SEQ_CST);
                } else {
                        //Genode::error("Core allocation failed.");
                        counter = 0;
                        ready = false;
                        latency = 0;
                        __atomic_store_n(&restart, true, __ATOMIC_SEQ_CST);
                }
            }
        }
        //Genode::log("Benchmak finished.");
    }

    void entry() override
    {
        /* We distinguish betweeen the main thread that allocates CPU cores and */
        if (_id == 0)
            benchmark_loop();
        else
            worker_loop(); /* and worker threads that are just used to measure wake-up latencies */
    }

    Cell(Libc::Env &env, Timer::Connection &timer, Genode::uint16_t id, Location const &location)
        : Thread(env, Name("test_", location.xpos(), "x", location.ypos()), 4 * 4096, location, Weight(), 
        env.cpu()), _id(id), env(env), _timer(timer)
    { }
};


void Libc::Component::construct(Libc::Env &env)
{
    genv = &env;

    CORES = cip->habitat_affinity.total() - 1; // cip->cores_reserved.count()-1;
    Genode::log("Starting.");

    Libc::with_libc(
        [&]()
        {
            Timer::Connection _timer{env};

            Genode::Heap _heap{env.ram(), env.rm()};

            try
            {
                Genode::Attached_rom_dataspace info(env, "platform_info");
                tsc_freq_khz = info.xml().sub_node("hardware").sub_node("tsc")
                                .attribute_value("freq_khz", 0ULL);
            }
            catch (...)
            {
            };

            Genode::Attached_rom_dataspace _config{env, "config"};
            Genode::Xml_node config = _config.xml();

            cores = config.attribute_value("start_with", 1);
            CORES = config.attribute_value("end_with", cip->habitat_affinity.total() - 1);
            CALLS = config.attribute_value("calls", 100);
            STEP = config.attribute_value("step", 4);

            //Genode::log("Running at a frequency of ", tsc_freq_khz, " KHz");

            start = Genode::Trace::timestamp();
            Genode::Thread *me = Genode::Thread::myself();

            Genode::Affinity::Space space = cip->habitat_affinity;

            pthread_t workers[space.total()];
            //std::cout << "Creating workers" << std::endl;
            //Genode::Trace::Timestamp thread_start = Genode::Trace::timestamp();
            
            Genode::Affinity::Location loc = me->affinity();

            //unsigned int cpu_id = cip->location_to_kernel_cpu(loc);
            //cip->cores_current.set(cpu_id);

            for (Genode::uint16_t cpu = 1; cpu < space.total(); cpu++)
            {
                Genode::String<32> const name{"worker", cpu};
                /* Do not create a worker for EP's CPU yet, because
                   we want to measure the time it takes to create only the worker threads */
                /*if (cpu == (space.total() - cpuid))
                    continue;*/
                bool repeat = true;
                do
                {
                    try
                    { 
                        Cell *worker = new Cell(env, _timer, cpu, space.location_of_index(cpu));
                        Libc::pthread_create_from_session(&workers[cpu], Cell::pthread_entry,
                                                  worker, 4 * 4096, name.string(), &env.cpu(), space.location_of_index(cpu));
                        repeat = false;
                    }
                    catch (Genode::Ipc_error &)
                    {
                        Genode::error("Failed to create worker. Retrying...");
                        repeat = true;
                    }
                } while (repeat);
            }

            //Genode::Trace::Timestamp thread_stop = Genode::Trace::timestamp();
            //Genode::log("Took ", (thread_stop - thread_start) / (tsc_freq_khz/1000), " μs to start workers");

            pthread_t main_pt{};

            bool repeat = true;
            //_timer.msleep(cip->location_to_kernel_cpu(loc)*80);
            // Genode::log("Starting main worker on CPU ", cip->location_to_kernel_cpu(loc));
            do
            {
                try {
                    Cell *main_cell = new Cell(env, _timer, 0, loc);

                    Libc::pthread_create_from_session(&main_pt, Cell::pthread_entry,
                         main_cell, 8 * 4096, "main_worker", &env.cpu(), loc);

                    //Genode::log("Waiting for main thread");
                    pthread_join(main_pt, 0);
                    repeat = false;
                } catch (Genode::Ipc_error &) {
                    Genode::error("Failed to create main worker. Retrying...");
                    repeat = true;
                }
            } while (repeat);
        });
    //Genode::log("Leaving component");
    env.parent().exit(0);
}
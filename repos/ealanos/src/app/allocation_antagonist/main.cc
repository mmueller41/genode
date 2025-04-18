/**
 * @file main.cc
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief Antagonist for Core allocation
 * @details This component acts as an antagonist to the allocating cell
 *          in allocating_cell by periodically allocating the maximum
 *          number of CPU cores, running in a loop for a specified time
 *          and then releasing the cores going to sleep for a certain
 *          amount of time. This way it will steal CPU cores from the 
 *          allocating cell as well as getting all its cores borrowed
 *          by the allocating cell, prompting core withdrawals.
 * @version 0.1
 * @date 2025-03-12
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <base/component.h>
#include <base/env.h>
#include <timer_session/connection.h>
#include <base/log.h>
#include <base/heap.h>
#include <tukija/syscalls.h>
#include "loop.h"

namespace Microbench {
    class Antagonist;
    class Worker;
}

void short_loop(unsigned long k)
{
    for (unsigned long i = 0; i < k; i++)
    {
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

class Microbench::Worker : public Genode::Thread {
    private:
        unsigned _id;
    
    public:
        void entry() override
        {
            Tukija::Cip::Worker *my_channel = &Tukija::Cip::cip()->worker_for_location(Genode::Thread::myself()->affinity());

            unsigned cpu_id = Tukija::Cip::cip()->location_to_kernel_cpu(Genode::Thread::myself()->affinity());

            Genode::log("Started antagonistic worker ", _id, " on CPU ", cpu_id);

            Tukija::release(Tukija::Resource_type::CPU_CORE);

            while(true) {
                while (!(my_channel->yield_flag))
                    short_loop(770);
                //Genode::log("Antagonistic worker on CPU ", cpu_id, " got yield request.");
                Tukija::return_to_owner(Tukija::Resource_type::CPU_CORE);
                //Genode::log("Reactivated antagonistic worker on CPU ", cpu_id);
            }
        }

        Worker(Genode::Env &env, unsigned id, Genode::Affinity::Location const &loc) 
            : Thread(env, Genode::Thread::Name("worker", _id), 4*4096, loc, Weight(), env.cpu()), _id(id) {}
};

class Microbench::Antagonist
{
    private:
        Genode::Env &_env;
        Timer::Connection _timer{_env};

    public:
        Antagonist(Genode::Env &env) : _env(env) 
        {
            Genode::Heap heap{_env.ram(), _env.rm()};

            unsigned cpu_id = Tukija::Cip::cip()->location_to_kernel_cpu(Genode::Thread::myself()->affinity());

            Genode::Affinity::Space const &space = Tukija::Cip::cip()->habitat_affinity;

            /* Create worker threads */
            for (unsigned cpu = 1; cpu < space.total(); cpu++) {
                Worker *worker = new (heap) Worker(env, cpu, space.location_of_index(cpu));
                worker->start();
            }

            _timer.msleep(cpu_id);

            unsigned cores_owned = Tukija::Cip::cip()->cores_reserved.count();
            Genode::log("Owning ", cores_owned," CPU cores: ", Tukija::Cip::cip()->cores_reserved);

            for (;;) {
                unsigned allocated = 0;
                do {
                    Tukija::alloc(Tukija::Resource_type::CPU_CORE, space.total() / 2);
                    allocated += Tukija::Cip::cip()->cores_new.count();
                } while (allocated < cores_owned);

                Genode::log("Antagonist got CPUs ", Tukija::Cip::cip()->cores_new);

                while (Tukija::Cip::cip()->cores_current.count() != cores_owned+21) {
                    __builtin_ia32_pause();
                }

                loop(5);
            }
        }
};

void Component::construct(Genode::Env &env) {
    static Microbench::Antagonist antagonist(env);
}
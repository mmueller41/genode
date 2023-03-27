/**
 * @file trace_pfc.cc
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief Tests for Genode wrappers around Performance counter syscalls in NOVA
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <base/trace/perf.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <x86intrin.h>

using namespace Genode;

int main(void)
{
    Trace::Performance_counter::Counter ctr_clflush, ctr_l2_latency, ctr_l2_requests, /*ctr_l3_miss,*/ ctr_l2_prefetch;

    try {
        ctr_clflush = Trace::Performance_counter::alloc_core();
        ctr_l2_latency = Trace::Performance_counter::alloc_core();
        ctr_l2_requests = Trace::Performance_counter::alloc_core();
        ctr_l2_prefetch = Trace::Performance_counter::acquire(Trace::Performance_counter::Type::CORE);
        // ctr_l3_miss = Trace::Performance_counter::alloc_cbo();
    }
    catch (Trace::Pfc_no_avail)
    {
        std::cout << "Unable to allocate performance counters." << std::endl;
        return -1;
    }

    std::cout << "Performance counter allocation successful." << std::endl;
    
    try {
        Trace::Performance_counter::setup(ctr_clflush, 0x26, 0x00, 0x70000);
        Trace::Performance_counter::setup(ctr_l2_latency, 0x62, 0x01, 0x30000);
        Trace::Performance_counter::setup(ctr_l2_requests, 0x60, 0xfe, 0x30000);
        Trace::Performance_counter::setup(ctr_l2_prefetch, 0xc0, 0x00, 0x30000);
        //Trace::Performance_counter::setup(ctr_l3_miss, 0x6, 0xff, 0x550f000000000000);
    } catch (Trace::Pfc_access_error &e) {
        std::cerr << "PFC access failed. rc=" << e.error_code() << std::endl;
        return -1;
    }

    std::cout << "Performance counters successfully set up." << std::endl;

    try {
        Trace::Performance_counter::start(ctr_clflush);
        Trace::Performance_counter::start(ctr_l2_latency);
        Trace::Performance_counter::start(ctr_l2_requests);
        Trace::Performance_counter::start(ctr_l2_prefetch);
        //Trace::Performance_counter::start(ctr_l3_miss);
    } catch (Trace::Pfc_access_error &e) {
        std::cerr << "PFC access failed. rc=" << e.error_code() << std::endl;
        return -1;
    }

    std::cout << "Performance counters started." << std::endl;

    for (;;) {
        Genode::uint64_t clflushes, latency, requests, /*l3_misses,*/ l2_prefetches;
        clflushes = latency = requests = l2_prefetches = 0;

        std::this_thread::sleep_for(std::chrono::seconds(2));
        _mm_clflush(&clflushes);
        _mm_clflush(&clflushes);

        try {
            clflushes = Trace::Performance_counter::read(ctr_clflush);
            latency = Trace::Performance_counter::read(ctr_l2_latency);
            requests = Trace::Performance_counter::read(ctr_l2_requests);
            l2_prefetches = Trace::Performance_counter::read(ctr_l2_prefetch);
            //l3_misses = Trace::Performance_counter::read(ctr_l3_miss);
        } catch (Trace::Pfc_access_error &e) {
            std::cerr << "PFC access failed. rc=" << e.error_code() << std::endl;
            return 1;
        }

        std::cout << clflushes << " cache line flushes." << std::endl;
        //std::cout << "L2 latency: " << (latency * 4) / requests << " cycles." << std::endl;
        std::cout << l2_prefetches << " L2 prefetch requests." << std::endl;
   /* 
        try {
            Trace::Performance_counter::stop(ctr_l2_prefetch);
            Trace::Performance_counter::reset(ctr_l2_prefetch, 0xdeadbeef);
            Trace::Performance_counter::start(ctr_l2_prefetch);
            std::cout << Trace::Performance_counter::read(ctr_l2_prefetch) << " L2 prefetches after context-switch" << std::endl;
            Trace::Performance_counter::stop(ctr_l2_prefetch);
            Trace::Performance_counter::reset(ctr_l2_prefetch, l2_prefetches);
            Trace::Performance_counter::start(ctr_l2_prefetch);
        } catch (Trace::Pfc_access_error &e) {
            std::cerr << "PFC access failed. rc=" << e.error_code() << std::endl;
        }
*/
        // std::cout << l3_misses << " L3 misses" << std::endl;
    }

    return 0;
}
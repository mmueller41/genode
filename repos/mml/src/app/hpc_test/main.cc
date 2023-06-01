/**
 * @file main.cc
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief Some test for programing hardware performance counters in NOVA
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <nova/syscall-generic.h>
#include <nova/syscalls.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <x86intrin.h>

int main(void) 
{
    Nova::mword_t event = 0x26;
    Nova::mword_t mask = 0x00;
    Nova::mword_t flags = 0x70000;
    Nova::uint8_t rc;

    if ((rc = Nova::hpc_ctrl(Nova::HPC_SETUP, 0, 1, event, mask, flags)) != Nova::NOVA_OK) {
        std::cerr << "Failed to setup performance counter 0" << std::endl;
        return -1;
    }

    std::cout << "Counter 0 setup" << std::endl;
    event = 0x60;
    mask = 0xfe;
    if ((rc = Nova::hpc_ctrl(Nova::HPC_SETUP, 1, 1, event, mask, flags)) != Nova::NOVA_OK)
    {
        std::cerr << "Failed to setup performance counter 1, rc = " <<  static_cast<Nova::uint32_t>(rc) << std::endl;
        return -1;
    }

    event = 0x62;
    mask = 0x1;
    if ((rc = Nova::hpc_ctrl(Nova::HPC_SETUP, 2, 1, event, mask, flags)) != Nova::NOVA_OK)
    {
        std::cerr << "Failed to setup performance counter 2, rc = " <<  static_cast<Nova::uint32_t>(rc) << std::endl;
        return -1;
    }
    if ((rc = Nova::hpc_start(0, 1)) != Nova::NOVA_OK) {
        std::cerr << "Failed to start counter 0" << std::endl;
        return -2;
    }
    
    if ((rc = Nova::hpc_start(1, 1)) != Nova::NOVA_OK) {
        std::cerr << "Failed to start counter 0" << std::endl;
        return -2;
    }

    if ((rc = Nova::hpc_start(2, 1)) != Nova::NOVA_OK) {
        std::cerr << "Failed to start counter 0" << std::endl;
        return -2;
    }

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        Nova::mword_t count = 0;
        
        _mm_clflush(&count);
        if ((rc = Nova::hpc_read(0, 1, count)) != Nova::NOVA_OK)
        {
            std::cerr << "Failed to read counter 0" << std::endl;
        }
        std::cout << count << " cache line flushes" << std::endl;

        Nova::mword_t latency = 0;
        if ((rc = Nova::hpc_read(2, 1, latency)) != Nova::NOVA_OK)
        {
            std::cerr << "Failed to read counter 1" << std::endl;
        }
        Nova::mword_t l2_requests = 0;
        if ((rc = Nova::hpc_read(1, 1, l2_requests)) != Nova::NOVA_OK)
        {
            std::cerr << "Failed to read counter 1" << std::endl;
        }
        count = (latency * 4) / l2_requests;
        std::cout << "L2 latency:" << count << " cycles" << std::endl;
    }

    return 0;
}

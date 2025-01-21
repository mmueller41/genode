/**
 * @file main.cc
 * @author Michael Müller (michael.mueller@uos.de)
 * @brief Some Tests for using Performance Counters with libpfm and the NOVA syscalls
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>

#include <nova/syscall-generic.h>
#include <nova/syscalls.h>

extern "C" {
#include <perfmon/err.h>
#include <perfmon/pfmlib.h>
}

int main(void)
{
    pfm_pmu_info_t pinfo;
    pfm_pmu_encode_arg_t e;
    pfm_event_info_t info;
    int ret;

    ret = pfm_initialize();
    if (ret != PFM_SUCCESS) {
        std::cerr << "cannot initialize libpfm: " << pfm_strerror(ret) << std::endl;
        return EXIT_FAILURE;
    }

    memset(&pinfo, 0, sizeof(pfm_pmu_info_t));

    ret = pfm_get_pmu_info(PFM_PMU_AMD64_FAM17H_ZEN1, &pinfo);
    if (ret != PFM_SUCCESS)
    {
        std::cerr << "Failed to find PMU" << std::endl;
        return -EXIT_FAILURE;
    }

    if (!pinfo.is_present) {
        std::cerr << "No AMD PMU present" << std::endl;
        return -EXIT_FAILURE;
    }

    memset(&e, 0, sizeof(e));

    char *fqstr = nullptr;
    e.fstr = &fqstr;

    do
    {
        ret = pfm_get_os_event_encoding("ITLB_RELOADS", PFM_PLM0 | PFM_PLM3, PFM_OS_NONE, &e);
        if (ret == PFM_ERR_TOOSMALL) {
            free(e.codes);
            e.codes = NULL;
            e.count = 0;
            continue;
        } else {
            std::cerr << "No such event" << std::endl;
            return EXIT_FAILURE;
        }
    } while (ret != PFM_SUCCESS);

    memset(&info, 0, sizeof(info));

    ret = pfm_get_event_info(e.idx, PFM_OS_NONE, &info);
    if (ret) {
        std::cerr << "Failed to get event info" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Event found : " << fqstr << std::endl;
    std::cout << "Code : " << info.code << std::endl;

    Nova::uint8_t rc = 0;
    Nova::mword_t umask = 0x6;
    Nova::mword_t flags = 0x0;

    if ((rc = Nova::hpc_ctrl(Nova::HPC_SETUP, 0, 1, info.code, umask, flags)) != Nova::NOVA_OK) {
        std::cerr << "Failed to setup HPC 0 for event" << std::endl;
        return EXIT_FAILURE;
    }

    if ((rc = Nova::hpc_start(0, 1))) {
        std::cerr << "Failed to start counter" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Successfully set up hardware performance counter 0" << std::endl;

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Nova::mword_t value;
        if ((rc = Nova::hpc_read(0, 1, value)) != Nova::NOVA_OK) {
            std::cerr << "Failed to read HPC" << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "Counter value: " << value << std::endl;
    }

    return EXIT_SUCCESS;
}
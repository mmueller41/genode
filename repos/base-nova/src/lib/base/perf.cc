
/*
 * \brief  Performance Counter infrastructure, NOVA-specific implemantation
 * \author Michael Müller
 * \date   2022-12-15
 */

#include <base/trace/perf.h>

#include <nova/syscall-generic.h>
#include <nova/syscalls.h>
#include <base/log.h>

unsigned long Genode::Trace::Performance_counter::private_freemask { 0xffff };
unsigned long Genode::Trace::Performance_counter::shared_freemask { 0xffff0000 };

void Genode::Trace::Performance_counter::_init_masks()
{
    Nova::Hip::Cpu_desc::Vendor vendor = Nova::Hip::Cpu_desc::AMD;
    if (vendor == Nova::Hip::Cpu_desc::AMD)
    {
        private_freemask = 0x3f; // 6 core performance counters
        shared_freemask = 0x1f0000; // 5 L3 complex performance counters
    }
    else if (vendor == Nova::Hip::Cpu_desc::INTEL)
    {
        private_freemask = 0x7fff;
        shared_freemask = 0x7fff0000; // 15 CBO performance counters
    }
}

void Genode::Trace::Performance_counter::setup(unsigned counter, uint64_t event, uint64_t mask, uint64_t flags)
{
    Nova::mword_t evt = event;
    Nova::mword_t msk = mask;
    Nova::mword_t flg = flags;
    Nova::uint8_t rc;
    Nova::mword_t type = (counter >>4);
    Nova::mword_t sel = type == Performance_counter::CORE ? counter : counter & 0xf;

    if ((rc = (Nova::hpc_ctrl(Nova::HPC_SETUP, sel, type, evt, msk, flg))) != Nova::NOVA_OK)
        throw  Genode::Trace::Pfc_access_error(rc);
}

void Genode::Trace::Performance_counter::start(unsigned counter)
{
    Nova::uint8_t rc;
    Nova::mword_t type = (counter >> 4);
    Nova::mword_t sel = type == Performance_counter::CORE ? counter : counter >>4;

    if ((rc = Nova::hpc_start(sel, type)) != Nova::NOVA_OK)
        throw  Genode::Trace::Pfc_access_error(rc);
}

void Genode::Trace::Performance_counter::stop(unsigned counter)
{
    Nova::uint8_t rc;
    Nova::mword_t type = (counter >>4);
    Nova::mword_t sel = type == Performance_counter::CORE ? counter : counter & 0xf;

    if ((rc = Nova::hpc_stop(sel, type)) != Nova::NOVA_OK)
        throw  Genode::Trace::Pfc_access_error(rc);
}

void Genode::Trace::Performance_counter::reset(unsigned counter, unsigned val)
{
    Nova::uint8_t rc;
    Nova::mword_t type = (counter >>4);
    Nova::mword_t sel = type == Performance_counter::CORE ? counter : counter & 0xf;

    if ((rc = Nova::hpc_reset(sel, type, val)) != Nova::NOVA_OK)
        throw  Genode::Trace::Pfc_access_error(rc);
}

Genode::uint64_t Genode::Trace::Performance_counter::read(unsigned counter)
{
    Nova::uint8_t rc;
    Nova::mword_t value = 0;
    Nova::mword_t type = (counter >>4);
    Nova::mword_t sel = type == Performance_counter::CORE ? counter : counter & 0xf;

    if ((rc = Nova::hpc_read(sel, type, value)) != Nova::NOVA_OK)
        throw  Genode::Trace::Pfc_access_error(rc);

    Genode::log("Performance_counter::read = ", value);
    return static_cast<Genode::uint64_t>(value);
}
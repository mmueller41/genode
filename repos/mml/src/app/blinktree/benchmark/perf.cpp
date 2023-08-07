#include "perf.h"

using namespace benchmark;

/**
 * Counter "Instructions Retired"
 * Counts when the last uop of an instruction retires.
 */
[[maybe_unused]] PerfCounter Perf::INSTRUCTIONS = {"instr", Genode::Trace::Performance_counter::Type::CORE, 0xc0, 0x0};

/**
 */
[[maybe_unused]] PerfCounter Perf::CYCLES = {"cycles", Genode::Trace::Performance_counter::Type::CORE, 0x76, 0x0};

/**
 */
[[maybe_unused]] PerfCounter Perf::L1_DTLB_MISSES = {"l1-dtlb-miss", Genode::Trace::Performance_counter::Type::CORE, 0x45, 0xff};
[[maybe_unused]] PerfCounter Perf::L1_ITLB_MISSES = {"l1-itlb-miss", Genode::Trace::Performance_counter::Type::CORE, 0x85, 0x0};

/**
 * Counter "LLC Misses"
 * Accesses to the LLC in which the data is not present(miss).
 */
[[maybe_unused]] PerfCounter Perf::LLC_MISSES = {"llc-miss", Genode::Trace::Performance_counter::Type::CACHE, 0x6, 0xff};

/**
 * Counter "LLC Reference"
 * Accesses to the LLC, in which the data is present(hit) or not present(miss)
 */
[[maybe_unused]] PerfCounter Perf::LLC_REFERENCES = {"llc-ref", Genode::Trace::Performance_counter::Type::CACHE, 0x4, 0xff};

/**
 * Micro architecture "Skylake"
 * Counter "CYCLE_ACTIVITY.STALLS_MEM_ANY"
 * EventSel=A3H,UMask=14H, CMask=20
 * Execution stalls while memory subsystem has an outstanding load.
 */
//PerfCounter Perf::STALLS_MEM_ANY = {"memory-stall", PERF_TYPE_RAW, 0x145314a3};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.NTA"
 * EventSel=32H,UMask=01H
 * Number of PREFETCHNTA instructions executed.
 */
[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_NTA = {"sw-prefetch-nta", Genode::Trace::Performance_counter::Type::CORE, 0x4b, 0x4};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.T0"
 * EventSel=32H,UMask=02H
 * Number of PREFETCHT0 instructions executed.
 */
//[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_T0 = {"sw-prefetch-t0", Genode::Trace::Performance_counter::Type::CORE, 0x4b, };

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.T1_T2"
 * EventSel=32H,UMask=04H
 * Number of PREFETCHT1 or PREFETCHT2 instructions executed.
 */
//[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_T1_T2 = {"sw-prefetch-t1t2", PERF_TYPE_RAW, 0x530432};

/**
 * Micro architecture "Skylake"
 * Counter "SW_PREFETCH_ACCESS.PREFETCHW"
 * EventSel=32H,UMask=08H
 * Number of PREFETCHW instructions executed.
 */
[[maybe_unused]] PerfCounter Perf::SW_PREFETCH_ACCESS_WRITE = {"sw-prefetch-w", Genode::Trace::Performance_counter::Type::CORE, 0x4b, 0x2};
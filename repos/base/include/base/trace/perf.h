/*
 * \brief  Performance Counter infrastructure
 * \author Michael Müller
 * \date   2022-12-15
 */

#pragma once

#include <base/stdint.h>

namespace Genode { namespace Trace {

    class Pfc_no_avail {
    };

    class Performance_counter
    {

    private:
        static unsigned long private_freemask;
        static unsigned long shared_freemask;

        static unsigned _alloc(unsigned long *free_mask)
        {
            unsigned long current_mask, new_mask;
            unsigned bit;

            do
            {
                current_mask = *free_mask;
                bit = __builtin_ffsl(current_mask);
                new_mask = current_mask & ~(1 << (bit - 1));
            } while (!__atomic_compare_exchange(free_mask, &current_mask, &new_mask, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));

            if (!bit) // Allocation failed
                throw Pfc_no_avail();

            return bit - 1; // number of the allocated counter
            }

            static void _init_masks();

        public:
            typedef unsigned int Counter;
            
            enum Type
            {
                CORE = 0,
                CACHE = 1
            };
            
            static unsigned acquire(Type type) {
                return (type == Type::CORE) ? alloc_core() : alloc_cbo();
            }

            static unsigned alloc_cbo() {
                if (shared_freemask == 0xffff0000)
                    _init_masks();
                return _alloc(&shared_freemask);
            }

            static unsigned alloc_core() {
                if (private_freemask == 0xffff)
                    _init_masks();
                return _alloc(&private_freemask);
            }

            static void release(unsigned counter) {
                bool core = static_cast<bool>(counter >> 4);
                if (core)
                    private_freemask |= (1 << counter);
                else
                    shared_freemask |= (1 << counter);
            }

            static void setup(unsigned counter, Genode::uint64_t event, Genode::uint64_t mask, Genode::uint64_t flags);
            static void start(unsigned counter);
            static void stop(unsigned counter);
            static void reset(unsigned counter, unsigned val=0);
            static uint64_t read(unsigned counter);
    };

    class Pfc_access_error {
        private:
            Genode::uint8_t _rc;

        public:
            Pfc_access_error(uint8_t rc) : _rc(rc) {}
            Genode::uint8_t error_code() { return _rc; }
    };

    }
}
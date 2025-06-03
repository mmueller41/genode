/*
 * Bit allocator
 *
 * Copyright (C) 2020 Alexander Boettcher, Genode Labs GmbH
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */


#pragma once

#include "bits.h"
#include "atomic.h"

namespace mx::util {
    template<unsigned C, unsigned INV> class Bit_alloc;
}

template<unsigned C, unsigned INV>
class mx::util::Bit_alloc
{
    private:

        alignas(64) std::uint64_t bits [C / 8 / sizeof(std::uint64_t)];
        std::uint64_t last { 0 };

        enum {
            BITS_CNT = sizeof(bits[0]) * 8,
            MAX      = sizeof(bits) / sizeof(bits [0])
        };

    public:

        
        inline std::uint64_t max() const { return C; }

        
        inline Bit_alloc()
        {
            static_assert(MAX*BITS_CNT == C, "bit allocator");
            static_assert(INV < C, "bit allocator");

            Atomic::test_set_bit(bits[INV / BITS_CNT], INV % BITS_CNT);
        }

        
        inline std::uint64_t alloc_with_mask(std::uint64_t bitmask[C/8/sizeof(std::uint64_t)], bool inverse = false)
        {
            for (std::uint64_t i = ACCESS_ONCE(last), j = 0; j < MAX; i++, j++)
            {
                i %= MAX;
                
                std::uint64_t mask = ~bits[i] & (inverse ? ~bitmask[i] : bitmask[i]);

                if (mask == 0UL)
                    continue;


                long b = bit_scan_forward (mask);
                if (b < 0 || b >= BITS_CNT || Atomic::test_set_bit (bits[i], b)) {
                    j--;
                    i--;
                    continue;
                }

                if (bits[i] != ~0UL && last != i)
                    last = i;

                return i * BITS_CNT + b;
            }

            return INV;
        }

        
        inline std::uint64_t alloc()
        {
            return alloc_with_mask(bits, true);
        }

        
        inline void release(std::uint64_t const id)
        {
            if (id == INV || id >= C)
                return;

            std::uint64_t i = id / BITS_CNT;
            std::uint64_t b = id % BITS_CNT;

            while (ACCESS_ONCE(bits[i]) & (1ul << b))
                 Atomic::test_clr_bit (ACCESS_ONCE(bits[i]), b);
        }

        
        inline bool reserve(std::uint64_t const id)
        {
            
            if (id == INV || id >= C)
                return false;

            std::uint64_t i = id / BITS_CNT;
            std::uint64_t b = id % BITS_CNT;

            return Atomic::test_set_bit (ACCESS_ONCE(bits[i]), b);
        }

        void reserve(std::uint64_t const start, std::uint64_t const count)
        {
            if (start >= C)
                return;

            std::uint64_t i = start / BITS_CNT;
            std::uint64_t b = start % BITS_CNT;

            std::uint64_t cnt = count > C ? C : count;
            if (start + cnt > C)
                cnt = C - start;

            while (cnt) {
                std::uint64_t const c  = (cnt > BITS_CNT) ? std::uint64_t(BITS_CNT) : cnt;
                std::uint64_t const bc = (c > (BITS_CNT - b)) ? std::uint64_t(BITS_CNT - b) : c;
                if (bits[i] != ~0UL) {
                    if (bc >= BITS_CNT) {
                        bits[i] = ~0UL;
                    } else {
                        bits[i] |= ((1ul << bc) - 1) << b;
                    }
                }
                i++;
                cnt -= bc;
                b = 0;
            }
        }

        void reserve_with_mask(std::uint64_t const mask, std::uint64_t const offset)
        {
            Atomic::set_mask(bits[offset], mask);
        }

        void dump_trace()
        {
            for (int i = 0; i < MAX; i++)
                trace(0, "bitmap[%d]: %lx", i, bits[i]);
        }
};

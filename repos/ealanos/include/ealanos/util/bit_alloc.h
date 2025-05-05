/*
 * Bit allocator
 *
 * Copyright (C) 2020 Alexander Boettcher, Genode Labs GmbH
 * Copyright (C) 2025 Michael Müller, Osnabrück University
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

#include <tukija/bits.h>
#include <tukija/atomic.h>
#include <tukija/stdint.h>
#include <base/stdint.h>
#include <base/log.h>

#define ACCESS_ONCE(x) (*static_cast<volatile typeof(x) *>(&(x)))

template<unsigned INV, unsigned C>
class Bit_alloc
{
    private:
        Tukija::mword_t bits[C / 8 / sizeof(Tukija::mword_t)];
        Tukija::mword_t last { 0 };
        const Genode::size_t _capacity;

        enum
        {
            BITS_CNT = sizeof(bits[0]) * 8,
            MAX = C / 8 / sizeof(Tukija::mword_t),
        };

    public:
        /**
         * @brief Return the number of "free", i.e. unset, bits
         * 
         * @return Tukija::mword_t 
         */
        inline Tukija::mword_t left() const
        {
            Tukija::mword_t count = 0;
            for (Tukija::mword_t i = 0; i < MAX; i++) {
                count += popcount(~bits[i]);
            }
            return count;
        }

        Genode::size_t capacity() const {
            return _capacity;
        }

        inline void print() const
        {
            for (Tukija::mword_t i = 0; i < MAX; i++) {
                Genode::log("bits[", i, "] = ", bits[i]);
            }
        }

        /**
         * @brief Return the maximum number of bits available.
         * 
         * @return Tukija::mword_t 
         */
        inline Tukija::mword_t max() const { return C; }

        inline Bit_alloc(Tukija::mword_t count) : _capacity(count)
        {
            static_assert(MAX * BITS_CNT == C, "bit allocator");
            static_assert(INV < C, "bit allocator");
            Atomic::test_set_bit(bits[INV / BITS_CNT], INV % BITS_CNT);
            reserve(count, C - count);
        }

        /**
         * @brief Allocate a bit
         * 
         * @return Tukija::mword_t - the index of the allocated bit
         */
        inline Tukija::mword_t alloc()
        {
            for (Tukija::mword_t i = ACCESS_ONCE(last), j = 0; j < MAX; i++, j++)
            {
                i %= MAX;

                if (ACCESS_ONCE(bits[i]) == ~0UL)
                    continue;

                long b = bit_scan_forward (~bits[i]);
                if (b < 0 || b >= BITS_CNT || Atomic::test_set_bit (bits[i], b)) {
                    j--;
                    i--;
                    continue;
                }

                if (bits[i] != ~0UL && last != i)
                    last = i;
                
                if (i*BITS_CNT+b >= _capacity)
                    return INV;

                return i * BITS_CNT + b;
            }

            return INV;
        }

        /**
         * @brief Release, i.e. clear, a bit
         * 
         * @param id - Bit to release to the allocator
         */
        inline void release(Tukija::mword_t const id)
        {
            if (id == INV || id >= _capacity)
                return;

            Tukija::mword_t i = id / BITS_CNT;
            Tukija::mword_t b = id % BITS_CNT;

            while (ACCESS_ONCE(bits[i]) & (1ul << b))
                 Atomic::test_clr_bit (ACCESS_ONCE(bits[i]), b);
        }

        /**
         * @brief Reserve a range of bits, thus, making them unavailable
         * 
         * @details Reserve a range of bits. This method can be used to
         *          limit the number of bits than can be allocated independent
         *          of the template parameter C. This is especially useful
         *          for cases, where more a consecutive range of bits is needed,
         *          or the number of bits allocatable shall be reduced to less
         *          than C. 
         * 
         * @param start - First bit from which the reservation shall start
         * @param count - Number of consecutive bits to reserve
         */
        void reserve(Tukija::mword_t const start, Tukija::mword_t const count)
        {
            if (start >= C)
                return;

            Tukija::mword_t i = start / BITS_CNT;
            Tukija::mword_t b = start % BITS_CNT;

            Tukija::mword_t cnt = count > C ? C : count;
            if (start + cnt > C)
                cnt = C - start;

            while (cnt) {
                Tukija::mword_t const c  = (cnt > BITS_CNT) ? Tukija::mword_t(BITS_CNT) : cnt;
                Tukija::mword_t const bc = (c > (BITS_CNT - b)) ? Tukija::mword_t(BITS_CNT - b) : c;
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

        /**
         * @brief Create a new Bit allocator at address p
         * 
         * @param p 
         * @return void* 
         */
        void *operator new(Genode::size_t, void *p) { return p; }
};

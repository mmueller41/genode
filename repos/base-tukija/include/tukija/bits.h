/* Bit manipulation functions taken from NOVA 
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2019-2024 Udo Steinberg, BlueRock Security, Inc.
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
#include <tukija/stdint.h>

inline long int bit_scan_forward (Tukija::mword_t val)
{
    if ((!val))
        return -1;

    asm volatile ("bsf %1, %0" : "=r" (val) : "rm" (val));

    return val;
}

inline Tukija::mword_t popcount(Tukija::mword_t bitset)
{
    return __builtin_popcountl(bitset);
}

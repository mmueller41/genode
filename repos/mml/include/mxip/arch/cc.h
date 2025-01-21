/*
 * \brief  Some size definitions and macros needed by LwIP.
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2009-11-10
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__ARCH__CC_H__
#define __LWIP__ARCH__CC_H__

#ifdef LITTLE_ENDIAN
#undef LITTLE_ENDIAN
#endif

#ifdef BIG_ENDIAN
#undef BIG_ENDIAN
#endif

#ifndef LWIP_RAND
genode_uint32_t genode_rand();
#define LWIP_RAND() genode_rand()
#endif

#include <stdint.h>
#include <limits.h>
#include <unistd.h>

#ifndef LWIP_PLATFORM_DIAG
void lwip_printf(const char *format, ...);
#define LWIP_PLATFORM_DIAG(x)   do { lwip_printf x; } while(0)
#endif /* LWIP_PLATFORM_DIAG */


#ifdef GENODE_RELEASE
#define LWIP_PLATFORM_ASSERT(x)
#else /* GENODE_RELEASE */
void lwip_platform_assert(char const* msg, char const *file, int line);
#define LWIP_PLATFORM_ASSERT(x)                           \
	do {                                                  \
		lwip_platform_assert(x, __FILE__, __LINE__);      \
	} while (0)
#endif /* GENODE_RELEASE */


/*
 * XXX: Should these be inlined?
 */
void  genode_memcpy( void *dst, const void *src, size_t len);
void *genode_memmove(void *dst, const void *src, size_t len);

void  genode_free(void *ptr);
void *genode_malloc(unsigned long size);
void *genode_calloc(unsigned long number, unsigned long size);

#define mem_clib_free   genode_free
#define mem_clib_malloc genode_malloc
#define mem_clib_calloc genode_calloc

#endif /* __LWIP__ARCH__CC_H__ */

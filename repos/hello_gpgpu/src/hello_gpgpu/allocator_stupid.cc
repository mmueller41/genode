#include "allocator_stupid.h"
#include <base/log.h>

using namespace Genode;

void* Allocator_stupid::alloc(size_t size)
{
    if(m_curr + size > m_end)
    {
        Genode::error("Allocator_stupid: OOM!");
        return nullptr;
    }

    const addr_t naddr = m_curr;
    m_curr += size;

    return (void*)naddr;
}

void* Allocator_stupid::alloc_aligned(uint32_t alignment, size_t size)
{
    m_curr = (m_curr + alignment - 1) & ~(alignment - 1);
    return alloc(size);
}

void Allocator_stupid::free(void* addr)
{
    (void)addr;
}

void Allocator_stupid::reset()
{
    m_curr = m_start;
}

void *operator new    (__SIZE_TYPE__ s, Genode::Allocator_stupid *a) { return a->alloc(s); }
void *operator new [] (__SIZE_TYPE__ s, Genode::Allocator_stupid *a) { return a->alloc(s); }
void *operator new    (__SIZE_TYPE__ s, Genode::Allocator_stupid &a) { return a.alloc(s); }
void *operator new [] (__SIZE_TYPE__ s, Genode::Allocator_stupid &a) { return a.alloc(s); }

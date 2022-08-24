// stdint
#include <base/fixed_stdint.h>
using namespace Genode;

// printk
#include <base/log.h>
#include <base/snprintf.h>
#include <util/string.h>

// genode instance
#include "gpgpu_genode.h"
extern gpgpu::gpgpu_genode* _global_gpgpu_genode;

// printing (optional)
extern "C" int printk(const char* str, ...)
{
    va_list list;
    va_start(list, str);

    char buff[256];
	String_console sc(buff, sizeof(buff));
	sc.vprintf(str, list);

    va_end(list);

    Genode::log("[GPU] ", Genode::Cstring(buff));
    return 0;
}

// allocator
extern "C" void* uos_aligned_alloc(uint32_t alignment, uint32_t size)
{
    return _global_gpgpu_genode->aligned_alloc(alignment, size);
}

extern "C" void free(void* addr)
{
    _global_gpgpu_genode->free(addr);
}

// pci
extern "C" uint32_t calculatePCIConfigHeaderAddress(uint8_t bus, uint8_t device, uint8_t function)
{
    _global_gpgpu_genode->createPCIConnection(bus, device, function);
	return 0;
}

extern "C" uint32_t readPCIConfigSpace(uint32_t addr)
{
    return _global_gpgpu_genode->readPCI((uint8_t)addr);
}

extern "C" void writePCIConfigSpace(uint32_t address, uint32_t value)
{
    _global_gpgpu_genode->writePCI((uint8_t)address, value);
}

// address model
extern "C" void* getVirtBarAddr(uint8_t bar_id)
{
    return (void*)_global_gpgpu_genode->getVirtBarAddr(bar_id);
}

extern "C" void* virt_to_phys(void* addr)
{
    return (void*)_global_gpgpu_genode->virt_to_phys((addr_t)addr);
}

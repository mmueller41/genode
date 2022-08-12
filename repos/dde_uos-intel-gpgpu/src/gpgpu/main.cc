#include <base/component.h>

#define GENODE // use genodes stdint header
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "gpgpu_genode.h"

//#define TEST // test stubs only (works with qemu)
#ifdef TEST
#include "../uos-intel-gpgpu/stubs.h"
#else
#include "test.h"
#endif // TEST

gpgpu_genode* _global_gpgpu_genode;

extern void construct_RPC(Genode::Env &env);

void Component::construct(Genode::Env& e)
{
    Genode::log("Hello world: UOS Intel GPGPU!");
    Genode::log("Build: ", __TIMESTAMP__);
    
    // init globals
    static gpgpu_genode gg(e);
    _global_gpgpu_genode = &gg;

    construct_RPC(e);

#ifdef TEST
    // test prink
    printk("Hello printk: %d", 42);

    // test alloc
    uint8_t* test = (uint8_t*)uos_aligned_alloc(0x1000, 0x1000);
    uint64_t addr = (uint64_t)test;
    if((addr & 0xFFF) != 0)
    {
        Genode::error("mem alignment failed: ", addr);
    }
    if(virt_to_phys(test) == nullptr)
    {
        Genode::error("mem phys addr NULL");
    }
    for(int i = 0; i < 0x1000; i++)
    {
        test[i] = 0x42;
    }
    for(int i = 0; i < 0x1000; i++)
    {
        if(test[i] != 0x42)
        {
            Genode::error("mem write or read failed!");
            break;
        }
    }
    free(test);
    Genode::log("Allocator test finished!");

    // test pci
    uint32_t base = calculatePCIConfigHeaderAddress(0, 2 , 0);
    uint32_t dev_ven = readPCIConfigSpace(base + 0);
    if((dev_ven & 0xFFFF) == 0x8086)
    {
        Genode::log("PCI test successful!");
    }
    else
    {
        Genode::error("PCI test failed!");
    }

    // test pci memory
    uint8_t* test2 = (uint8_t*)_global_gpgpu_genode->getVirtBarAddr(0);
    test2[0x42] = 0x42;
    Genode::log("PCI memory test finished!");

    // test interrupts
    _global_gpgpu_genode->registerInterruptHandler();
    Genode::log("Interrupt test finished!");
#else
    // init driver
    GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
    gpgpudriver.init(0);
    _global_gpgpu_genode->registerInterruptHandler();

    // run the test and hope the best
    run_gpgpu_test();
#endif // TEST

    Genode::log("This is the UOS Intel GPGPU End!");
}

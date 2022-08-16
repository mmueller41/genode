#include "gpgpu_genode.h"

#define GENODE // use genodes stdint header
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"

void gpgpu_genode::handleInterrupt()
{
    // handle the gpu interrupt
    GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
    gpgpudriver.handleInterrupt();
    gpgpudriver.runNext();

    // ack the irq
    irq->ack_irq();
}

gpgpu_genode::gpgpu_genode(Env& e) : env(e), heap{ e.ram(), e.rm() }, alloc(&heap), ram_cap(), mapped_base(0), base(0), pci(e), dev(), prev_dev(), irq(nullptr), dispatcher(env.ep(), *this, &gpgpu_genode::handleInterrupt)
{
    // size of avaible memory for allocator
    const unsigned long size = 0x1000 * 0x1000;

    // allocate chunk of ram
    //ram_cap = e.ram().alloc(size);
    size_t donate = size;
    ram_cap =
        retry<Out_of_ram>(
            [&] () {
                return retry<Out_of_caps>(
                    [&] () { return pci.alloc_dma_buffer(size, UNCACHED); },
                    [&] () { pci.upgrade_caps(2); });
            },
            [&] () {
                pci.upgrade_ram(donate);
                donate = donate * 2 > size ? 4096 : donate * 2;
            });
    mapped_base = e.rm().attach(ram_cap);
    base = pci.dma_addr(ram_cap);
    //base = Dataspace_client(ram_cap).phys_addr();

    // use this ram for allocator
    alloc.add_range(mapped_base, size);
}

gpgpu_genode::~gpgpu_genode()
{
    // release pci dev and free allocator memory
    pci.release_device(dev);
    env.ram().free(ram_cap);
}

void* gpgpu_genode::aligned_alloc(uint32_t alignment, uint32_t size)
{
    if(alignment == 0x1000)
    {
        alignment = 12;
    }
    else if(alignment != 0x0)
    {
        Genode::error("[GPU] Unsupported alignment: ", alignment);
        return nullptr;
    }

    return alloc.alloc_aligned(size, alignment).convert<void *>(

		[&] (void *ptr) { return ptr; },

		[&] (Genode::Range_allocator::Alloc_error) -> void * {
            Genode::error("[GPU] Error in driver allocation!");
            return nullptr; 
        }
    );
}

void gpgpu_genode::free(void* addr)
{
    alloc.free(addr);
}

void gpgpu_genode::createPCIConnection(uint8_t bus, uint8_t device, uint8_t function)
{
    // get first device
    pci.with_upgrade([&] () { dev = pci.first_device(); });

    while (dev.valid()) {
        // release old one
        pci.release_device(prev_dev);
        prev_dev = dev;

        // get next one
        pci.with_upgrade([&] () { dev = pci.next_device(dev); });

        // check if this is the right one
        Platform::Device_client client(dev);
        uint8_t b, d, f;
        client.bus_address(&b, &d, &f);
        if(b == bus && d == device && f == function)
        {
            break;
        }
    }

    // we did not find the right one
    if (!dev.valid())
    {
        Genode::error("[GENODE_GPGPU]: Could not find PCI dev: ", bus, device, function);
        return;
    }
}

uint32_t gpgpu_genode::readPCI(uint8_t addr)
{
    Platform::Device_client client(dev);
    return client.config_read(addr, Platform::Device::ACCESS_32BIT);
}

void gpgpu_genode::writePCI(uint8_t addr, uint32_t val)
{
    Platform::Device_client client(dev);
    pci.with_upgrade([&] () {
        client.config_write(addr, val, Platform::Device::ACCESS_32BIT);
    });
}

addr_t gpgpu_genode::getVirtBarAddr(uint8_t bar_id) const
{
    // get virt bar id (why does this exist?)
    Platform::Device_client dc(dev);
    Platform::Device::Resource res = dc.resource(bar_id);
    uint8_t genodeBarID = dc.phys_bar_to_virt(bar_id);

    // create io mem session
    Genode::Io_mem_session_capability cap = dc.io_mem(genodeBarID);
    if (!cap.valid())
    {
        Genode::error("[GENODE_GPGPU]: IO memory session is not valid");
        return 0;
    }

    // get dataspace cap
    Genode::Io_mem_session_client mem(cap);
    Genode::Io_mem_dataspace_capability mem_ds(mem.dataspace());
    if (!mem_ds.valid())
    {
        Genode::error("[GENODE_GPGPU]: IO mem dataspace cap not valid");
        return 0;
    }

    // add addr to rm and get virt addr
    addr_t vaddr = env.rm().attach(mem_ds);
    vaddr |= res.base() & 0xfff;
    return vaddr;
}

void gpgpu_genode::registerInterruptHandler()
{
    Platform::Device_client client(dev);
    static Irq_session_client irq_client(client.irq(0)); // 0 ??
    irq = &irq_client;

    // set dispatcher
    irq->sigh(dispatcher);

    // initial ack
    irq->ack_irq();
}

addr_t gpgpu_genode::mapMemory(Genode::Ram_dataspace_capability& ram_cap_vm)
{
    return env.rm().attach(ram_cap_vm);
}

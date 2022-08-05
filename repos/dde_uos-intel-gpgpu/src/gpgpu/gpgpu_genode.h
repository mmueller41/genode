#ifndef GPGPU_GENODE_H
#define GPGPU_GENODE_H

// stdint
#include <base/fixed_stdint.h>
using namespace Genode;

// allocator
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <dataspace/client.h>

// pci
#include <legacy/x86/platform_session/connection.h>
#include <legacy/x86/platform_device/client.h>
#include <io_mem_session/connection.h>
#include <io_port_session/connection.h>

// interrupts
#include <irq_session/connection.h>

class gpgpu_genode
{
private:
    // genode enviroment
    Env& env;

    // allocator
    Heap heap;
    Allocator_avl alloc;
    Ram_dataspace_capability ram_cap;
    addr_t mapped_base;
    addr_t base;

    // pci
	Platform::Connection pci;
	Platform::Device_capability dev;
	Platform::Device_capability prev_dev;
    
    // interrupts
    Irq_session_client* irq;
    Signal_handler<gpgpu_genode> dispatcher;

    // do not allow copies
    gpgpu_genode(const gpgpu_genode& copy) = delete;
    gpgpu_genode& operator=(const gpgpu_genode& src) = delete;

    /**
     * @brief Interrupt handler
     * 
     */
    void handleInterrupt();

public:
    /**
     * @brief Construct a new gpgpu genode object
     * 
     * @param e 
     */
    gpgpu_genode(Env& e);

    /**
     * @brief Destroy the gpgpu genode object
     * 
     */
    ~gpgpu_genode();

    /**
     * @brief allocate aligned memory
     * 
     * @param alignment the alignment
     * @param size the size in bytes
     * @return void* the address of the allocated memory
     */
    void* aligned_alloc(uint32_t alignment, uint32_t size);

    /**
     * @brief free memory
     * 
     * @param addr the address of the memory to be freed
     */
    void free(void* addr);

    /**
     * @brief converts a virtual address into a physical address
     * 
     * @param virt the virtual address
     * @return addr_t the physical address
     */
    addr_t virt_to_phys(addr_t virt) const
    {
		return virt - mapped_base + base;
    }

    /**
     * @brief creates a connection to the PCI device. This has to be called before any read/write to the PCI device!
     * 
     * @param bus the bus id
     * @param device the device id
     * @param function the function id
     */
    void createPCIConnection(uint8_t bus, uint8_t device, uint8_t function);

    /**
     * @brief read from pci config space
     * 
     * @param addr the address to read from
     * @return uint32_t the value
     */
    uint32_t readPCI(uint8_t addr);

    /**
     * @brief write to pci config space (some register are protected by genode!)
     * 
     * @param addr the address to write to
     * @param val the value to write
     */
    void writePCI(uint8_t addr, uint32_t val);

    /**
     * @brief Get the Virt Bar Addr object
     * 
     * @param bar_id 
     * @return addr_t 
     */
    addr_t getVirtBarAddr(uint8_t bar_id) const;

    /**
     * @brief register the interrupt handler for the current PCI device
     * 
     */
    void registerInterruptHandler();
};

#endif // GPGPU_GENODE_H

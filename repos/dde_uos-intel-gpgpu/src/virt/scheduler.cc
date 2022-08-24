#include "scheduler.h"
#include "kernel.h"

// genode instance
#include "../gpgpu/gpgpu_genode.h"
extern gpgpu::gpgpu_genode* _global_gpgpu_genode;

// driver
#define GENODE
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "../uos-intel-gpgpu/driver/ppgtt32.h"

namespace gpgpu_virt
{

void Scheduler::schedule_next()
{
    VGpu* first = nullptr;
    do
    {
        VGpu* next;
        if ((next = static_cast<VGpu*>(_run_list.first())))
        {
            // set vgpu
            _curr_vgpu = next;

            // move vgpu to end of list
            _run_list.remove(next);
            _run_list.insert(next);

            // complete iteration?
            if(first == next)
            {
                return;
            }

            // remember start of our search
            if(first == nullptr)
            {
                first = next;
            }
        }
        else
        {
            _curr_vgpu = nullptr; 
        }
    }
    while(_curr_vgpu != nullptr && !_curr_vgpu->has_kernel()); // continue search if we picked a vgpu without kernel
}

void Scheduler::handle_gpu_event()
{
    // reduce frequency
    GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
    gpgpudriver.setMinFreq();

    /* Switch to next vGPU in the run list */
    schedule_next();

    // If no vGPU to schedule, this means that we don't have any clients with anymore.
    if(_curr_vgpu == nullptr)
    {
        idle = true;
        return;
    } 

    Kernel* next = _curr_vgpu->take_kernel();
    if(next == nullptr)
    {
        idle = true;
        return;
    }

    idle = false;

    // switch context
    dispatch(*_curr_vgpu);

    // set frequency
    gpgpudriver.setMaxFreq();

    // run gpgpu task
    gpgpudriver.enqueueRun(*next->get_config());

    // free kernel object
    // kernel_config will not be freed, just the Queue object!
    _global_gpgpu_genode->free(next);
}

}

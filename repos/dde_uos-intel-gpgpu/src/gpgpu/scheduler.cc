#include "scheduler.h"
#include "kernel.h"
#define GENODE
#include "../uos-intel-gpgpu/driver/gpgpu_driver.h"
#include "../uos-intel-gpgpu/driver/ppgtt32.h"

void gpgpu::Scheduler::schedule_next()
{
    VGpu *next;
    if ((next = static_cast<VGpu*>(_run_list.first()))) {
        this->dispatch(*next);
        _curr_vgpu = next;

        // move vgpu to end of list
        _run_list.remove(next);
        _run_list.insert(next);
    } else
        _curr_vgpu = nullptr; 
}

void gpgpu::Scheduler::handle_gpu_event()
{
    // reduce frequency
    GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
    gpgpudriver.setMinFreq();

    /* Switch to next vGPU in the run list */
    schedule_next();

    /* If no vGPU to schedule, this means that we don't have any clients anymore.
        * Thus, there are also no kernels anymore to run. */
    if (_curr_vgpu == nullptr) 
        return;

    Kernel *next = _curr_vgpu->take_kernel();

    if (!next) /* If there is no kernel for the vGPU left */
    {
        // TODO: search for kernels in vgpu list
        return;
    }
    
    // set frequency
    gpgpudriver.setMaxFreq();

    // run gpgpu task
    gpgpudriver.enqueueRun(*next->get_config());
}

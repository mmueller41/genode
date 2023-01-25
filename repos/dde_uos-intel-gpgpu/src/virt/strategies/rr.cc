#include "rr.h"

namespace gpgpu_virt
{

void RoundRobin::addVGPU(VGpu* vgpu)
{
    _run_list.enqueue((util::WFQueue::Chain*)vgpu);
}

void RoundRobin::removeVGPU(VGpu* vgpu)
{
    _run_list.remove((util::WFQueue::Chain*)vgpu);
}

VGpu* RoundRobin::nextVGPU()
{
    // list empty?
    if(_run_list.empty())
        return nullptr;

    VGpu* first = nullptr;
    VGpu* next = nullptr;
    for(;;)
    {
        // get next vgpu
        next = (VGpu*)_run_list.dequeue();

        // add vgpu back to end of list
        _run_list.enqueue((util::WFQueue::Chain*)next);
        
        // check if it has kernel?
        if(next->has_kernel())
        {
            return next;
        }

        // complete iteration?
        if(first == next)
        {
            break;
        }

        // set start of search
        if(first == nullptr)
        {
            first = next;
        }
    }

    // no vgpu with kernel found
    return nullptr;
}

}

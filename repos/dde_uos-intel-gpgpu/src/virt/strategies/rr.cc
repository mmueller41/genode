#include "rr.h"

namespace gpgpu_virt
{

void RoundRobin::addVGPU(VGpu* vgpu)
{
    _run_list.enqueue(*vgpu);
}

void RoundRobin::removeVGPU(VGpu* vgpu)
{
    _run_list.remove(*vgpu);
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
        _run_list.dequeue([&next](VGpu& vgpu){
            next = &vgpu;
        });

        // add vgpu back to end of list
        _run_list.enqueue(*next);
        
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

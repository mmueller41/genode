#include "cfs.h"

// genode instance
#include "../../gpgpu/gpgpu_genode.h"
extern gpgpu::gpgpu_genode* _global_gpgpu_genode;

namespace gpgpu_virt
{

static unsigned long long int rdtsc()
{
    unsigned long long int ret = 0;
    unsigned int cycles_lo;
    unsigned int cycles_hi;

    __asm__ volatile ("RDTSC" : "=a" (cycles_lo), "=d" (cycles_hi));
    ret = (unsigned long long int)cycles_hi << 32 | cycles_lo;

    return ret;
}

void CompletlyFair::addVGPU(VGpu* vgpu)
{
    // create new entry
    cfs_entry* ce = (cfs_entry*)vgpu;

    // find current min
    cfs_entry* min = rbt_ready.getMin_cached();

    // add new entry with minimum runtime
    if(min != nullptr)
    {
        ce->runtime = min->runtime;
    }

    // add vgpu to rbtree
    if(vgpu->has_kernel())
    {
        rbt_ready.insert(*ce);
    }
    else
    {
        rbt_idle.insert(*ce);
    }

}

void CompletlyFair::removeVGPU(VGpu* vgpu)
{
    if((VGpu*)_curr == vgpu || vgpu->has_kernel() )
    {
        rbt_ready.remove(*vgpu);
    }
    else
    {
        rbt_idle.remove(*vgpu);
    }
}

void CompletlyFair::updateVGPU(VGpu* vgpu)
{
    // dont touch a running vgpu! it will be updated in nextVGPU
    if(vgpu == (VGpu*)_curr)
    {
        return;
    }

    rbt_idle.remove(*vgpu);
    rbt_ready.insert(*vgpu);
}

VGpu* CompletlyFair::nextVGPU()
{
    // update cfs entry
    if(_curr != nullptr)
    {
        VGpu* curr = (VGpu*)_curr;
        int prio = curr->getPriority();
        _curr->runtime += (rdtsc() - _curr->ts) * -prio;

        if(curr->has_kernel())
        {
            rbt_ready.insert(*_curr);
        }
        else
        {
            rbt_idle.insert(*_curr);
        }
    }

    VGpu* min = (VGpu*)rbt_ready.getMin_cached();

    // tree empty?
    if(min == nullptr)
    {
        _curr = nullptr;
        return nullptr;
    }

    // remove vgpu from ready tree
    rbt_ready.remove(*min);

    // exec min
    _curr = (cfs_entry*)min;
    _curr->ts = rdtsc();
    return min;
}

}

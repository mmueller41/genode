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
    cfs_entry* ce = new (_global_gpgpu_genode->getAlloc()) cfs_entry(vgpu);

    // find current min
    cfs_entry* min = nullptr;
    _run_list.head([&min](cfs_entry& ce){
        min = &ce;
    });
    _run_list.for_each([&min](cfs_entry& ce){
        if(ce.runtime < min->runtime)
        {
            min = &ce;
        }
    });

    // add new entry with minimum runtime
    ce->runtime = min->runtime;
    _run_list.enqueue(*ce);
}

void CompletlyFair::removeVGPU(VGpu* vgpu)
{
    _run_list.for_each([&vgpu, this](cfs_entry& ce){
        if(ce.vgpu == vgpu)
        {
            _run_list.remove(ce);
            _global_gpgpu_genode->free(&ce);
        }
    });
}

VGpu* CompletlyFair::nextVGPU()
{
    // update cfs entry
    _curr->runtime += (rdtsc() - _curr->ts) * -_curr->vgpu->getPriority();

    // list empty?
    if(_run_list.empty())
        return nullptr;

    cfs_entry* min = nullptr;
    _run_list.head([&min](cfs_entry& ce){
        min = &ce;
    });

    _run_list.for_each([&min](cfs_entry& ce){
        if(ce.runtime < min->runtime && ce.vgpu->has_kernel())
        {
            min = &ce;
        }
    });

    if(min->vgpu->has_kernel()) // in case this is still the head
    {
        _curr = min;
        _curr->ts = rdtsc();
        return min->vgpu;
    }

    // no vgpu with kernel found
    return nullptr;
}

}

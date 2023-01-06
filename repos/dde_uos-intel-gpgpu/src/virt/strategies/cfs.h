#ifndef CFS_H
#define CFS_H

#include <util/fifo.h>
#include "../strategie.h"

namespace gpgpu_virt {

    class CompletlyFair : Strategie
    {
        class cfs_entry : public Genode::Fifo<cfs_entry>::Element
        {
            public:
                VGpu* vgpu;
                unsigned long runtime;
                unsigned long ts;
                cfs_entry(VGpu* vg) : vgpu(vg), runtime(0), ts(0) {} 
        };
        
        private:
            Genode::Fifo<cfs_entry> _run_list; // TODO: Red Black Tree
            cfs_entry* _curr;

            CompletlyFair(const CompletlyFair &copy) = delete;
            CompletlyFair(CompletlyFair&&) = delete;
            CompletlyFair& operator=(const CompletlyFair&) = delete;
            CompletlyFair& operator=(CompletlyFair&&) = delete;     
        
        public:
            CompletlyFair() : _run_list(), _curr(nullptr) {};

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void addVGPU(VGpu* vgpu) override;

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            void removeVGPU(VGpu* vgpu) override;

            /**
             * @brief 
             * 
             * @return VGpu* 
             */
            VGpu* nextVGPU() override;
    };
}

#endif // CFS_H

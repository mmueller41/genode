#ifndef RR_H
#define RR_H

#include "util/wf_queue.h"
#include "../strategie.h"

namespace gpgpu_virt {

    class RoundRobin : Strategie
    {
        private:
            util::WFQueue _run_list;
        
        public:
            RoundRobin() : _run_list() {};

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
             * @param vgpu 
             */
            void updateVGPU(VGpu* vgpu) override { (void)vgpu; }; // not needed

            /**
             * @brief 
             * 
             * @return VGpu* 
             */
            VGpu* nextVGPU() override;
    };
}

#endif // RR_H

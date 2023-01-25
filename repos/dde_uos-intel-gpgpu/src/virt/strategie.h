#ifndef STRATEGIE_H
#define STRATEGIE_H

#include "vgpu.h"

namespace gpgpu_virt {

    class Strategie
    {
        private:
	        Strategie(const Strategie &copy) = delete;

        public:
            Strategie() {};
            virtual ~Strategie() {};

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            virtual void addVGPU(VGpu* vgpu) = 0;

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            virtual void removeVGPU(VGpu* vgpu) = 0;

            /**
             * @brief 
             * 
             * @param vgpu 
             */
            virtual void updateVGPU(VGpu* vgpu) = 0;

            /**
             * @brief 
             * 
             * @return VGpu* 
             */
            virtual VGpu* nextVGPU() = 0;
    };
}

#endif // STRATEGIE_H

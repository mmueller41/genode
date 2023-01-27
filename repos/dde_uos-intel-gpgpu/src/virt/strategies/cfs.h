#ifndef CFS_H
#define CFS_H

#include "cfs_entry.h"
#include "util/guarded_rbtree.h"
#include "../strategie.h"

namespace gpgpu_virt {

    class CompletlyFair : Strategie
    {
        private:
            static int compareCFSentry(const cfs_entry& a, const cfs_entry& b)
            {
                return (int)((long)a.runtime - (long)b.runtime);
            }
            static int compareAddr(const cfs_entry& a, const cfs_entry& b)
            {
                return (int)(&a - &b);
            }
        
            util::Guarded_RBTree<cfs_entry> rbt_ready;
            util::Guarded_RBTree<cfs_entry> rbt_idle;
            cfs_entry* _curr;

            CompletlyFair(const CompletlyFair &copy) = delete;
            CompletlyFair(CompletlyFair&&) = delete;
            CompletlyFair& operator=(const CompletlyFair&) = delete;
            CompletlyFair& operator=(CompletlyFair&&) = delete;     
        
        public:
            CompletlyFair() : rbt_ready(compareCFSentry), rbt_idle(compareAddr), _curr(nullptr) {};

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
            void updateVGPU(VGpu* vgpu) override;

            /**
             * @brief 
             * 
             * @return VGpu* 
             */
            VGpu* nextVGPU() override;
    };
}

#endif // CFS_H

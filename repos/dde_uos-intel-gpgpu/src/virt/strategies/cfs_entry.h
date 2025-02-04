#ifndef CFS_ENTRY_H
#define CFS_ENTRY_H

#include "util/rbtree.h"

namespace gpgpu_virt {

    class cfs_entry : public util::RBTree<cfs_entry>::RBNode
    {
        public:
            unsigned long long runtime;
            unsigned long long ts;
            cfs_entry() : runtime(0), ts(0) {}
    };
}

#endif // CFS_ENTRY_H

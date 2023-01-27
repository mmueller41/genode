#ifndef GUARDED_RB_TREE_H
#define GUARDED_RB_TREE_H

#include "rbtree.h"
#include <base/mutex.h>

namespace gpgpu_virt::util {

    /**
     * @brief 
     * 
     * @tparam T must inherit from RBTree::RBNode class 
     */
    template <typename T>
    class Guarded_RBTree : public RBTree<T>
    {
        /// @brief Mutex to secure tree operations
        Genode::Mutex mtx;

        public:
            /**
             * @brief Construct a new Guarded_RBTree
             * 
             * @param comp_func function to compare two elements
             */
            Guarded_RBTree(int (*comp_func)(const T&, const T&)) : RBTree<T>(comp_func), mtx() { }

            /**
             * @brief Inserts an element into the tree
             * 
             * @param data the element to be inserted
             */
            void insert(T& data)
            {
                mtx.acquire();
                RBTree<T>::insert(data);
                mtx.release();
            }

            /**
             * @brief Deletes an element from the tree
             * 
             * @param data the element to be deleted
             */
            void remove(T& data)
            {
                mtx.acquire();
                RBTree<T>::remove(data);
                mtx.release();
            }

    };
}

#endif // GUARDED_RB_TREE_H

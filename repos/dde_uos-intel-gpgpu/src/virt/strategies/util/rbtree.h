#ifndef RBTREE_H
#define RBTREE_H

namespace gpgpu_virt::util {

    /**
     * @brief 
     * 
     * @tparam T must inherit from RBTree::RBNode class 
     */
    template <typename T>class RBTree
    {
        public:
            /// @brief color of a node
            enum color { BLACK, RED };

            /// @brief position of a node
            enum dir { LEFT = 0, RIGHT = 1 };

            /**
             * @brief Node data structure
             * 
             */
            struct RBNode
            {
                /// @brief pointer to parent node
                T *parent;

                /// @brief pointers to left and right child
                T *child[2];

                /// @brief color
                color c;

                /**
                 * @brief Construct a new RBNode
                 * 
                 */
                RBNode() : parent(nullptr), child{nullptr, nullptr}, c(color::RED) {}

                /**
                 * @brief Checks whether the node is the left or right child of its parent
                 * 
                 * @return true left child
                 * @return false right child
                 */
                bool isLeft() const
                {
                    return parent != nullptr && parent->child[dir::LEFT] == this;
                }
            };

        private:
            RBTree(const RBTree &copy) = delete;
            RBTree(RBTree&&) = delete;
            RBTree& operator=(const RBTree&) = delete;
            RBTree& operator=(RBTree&&) = delete;
        
            /// @brief the root element
            T *root;
            
            /// @brief cached minimum element
            T *min;

            /// @brief compare function provided by user
            int (*compare_func)(const T&, const T&);

            /**
             * @brief rotates a subtree into a direction
             * 
             * @param subtree the subtree to rotate
             * @param d the direction to rotate into
             */
            void rotate(T *subtree, const dir d)
            {
                T *grandparent = subtree->parent;
                T *sibling = subtree->child[1 - d];
                T *closenephew = sibling->child[d];

                subtree->child[1 - d] = closenephew;
                if(closenephew != nullptr) closenephew->parent = subtree;

                sibling->child[d] = subtree;
                subtree->parent = sibling;

                sibling->parent = grandparent;

                if(grandparent == nullptr)
                    root = sibling;
                else
                    grandparent->child[subtree == grandparent->child[0] ? dir::LEFT : dir::RIGHT] = sibling;
            }

            /**
             * @brief Nulls a nodes pointers (parent and children)
             * 
             * @param node the node to be nulled
             */
            void nullRBNode(RBNode *node)
            {
                node->parent = nullptr;
                node->child[dir::LEFT] = nullptr;
                node->child[dir::RIGHT] = nullptr;
            }

        public:
            /**
             * @brief Construct a new RBTree
             * 
             * @param comp_func function to compare two elements
             */
            RBTree(int (*comp_func)(const T&, const T&)) : root(nullptr), min(nullptr), compare_func(comp_func) {};

            /**
             * @brief Checks whether the tree is emtpy or not
             * 
             * @return true tree is emptry
             * @return false tree is no empty
             */
            bool isEmpty() const { return root == nullptr; }

            /**
             * @brief Inserts an element into the tree
             * 
             * @param data the element to be inserted
             */
            void insert(T& data)
            {
                T *new_node = &data;

                // update min
                if((min && compare_func(data, *(min)) < 0) || !min)
                    min = new_node;

                // case: 0
                if(root == nullptr)
                {
                    root = new_node;
                    new_node->c = color::BLACK;
                    return;
                }

                // BST insert
                {
                    T* parent = root;
                    T* node = root;
                    while(node != nullptr)
                    {
                        parent = node;
                        node = node->child[compare_func(data, *(node)) < 0 ? dir::LEFT : dir::RIGHT];
                    }
                    parent->child[compare_func(data, *(parent)) < 0 ? dir::LEFT : dir::RIGHT] = new_node;
                    new_node->parent = parent;
                    new_node->c = color::RED;
                }

                // fix the tree
                T* node = new_node;
                T* parent = node->parent;
                do
                {
                    // case 1
                    if(parent->c == color::BLACK)
                        return;

                    // check case 4
                    T* grandparent = parent->parent;
                    if(grandparent == nullptr)
                    {
                        // case 4
                        parent->c = color::BLACK;
                        return;
                    }
                    
                    // check case 5 or 6
                    dir parent_dir = parent->isLeft() ? dir::LEFT : dir::RIGHT;
                    T* uncle = grandparent->child[1 - parent_dir];
                    if(uncle == nullptr || uncle->c == color::BLACK)
                    {
                        // case 5
                        if(node == parent->child[1 - parent_dir])
                        {
                            rotate(parent, parent_dir);
                            node = parent;
                            parent = parent->parent;
                        }

                        // case 6
                        rotate(grandparent, (dir)(1 - parent_dir));
                        parent->c = color::BLACK;
                        grandparent->c = color::RED;
                        return;
                    }

                    // case 2
                    parent->c = color::BLACK;
                    uncle->c = color::BLACK;
                    grandparent->c = color::RED;
                    node = grandparent;
                }
                while((parent = node->parent) != nullptr);

                // case 3
                return;
            }

            /**
             * @brief Deletes an element from the tree
             * 
             * @param data the element to be deleted
             */
            void remove(T& data)
            {
                T* node = &data;
                T* lchild = node->child[dir::LEFT];
                T* rchild = node->child[dir::RIGHT];

                // case 7
                if(node == root && lchild == nullptr && rchild == nullptr)
                {
                    root = nullptr;
                    min = nullptr;
                    nullRBNode(&data);
                    return;
                }

                // update min
                if(min == node)
                {
                    min = min->child[dir::RIGHT] ? min->child[dir::RIGHT] : node->parent;
                    if(min == nullptr)
                    {
                        min = root;
                    }
                }

                // case 8
                if(lchild != nullptr && rchild == nullptr ||
                    lchild == nullptr && rchild != nullptr)
                {
                    if(node != root)
                    {
                        T* child = lchild ? lchild : rchild;
                        node->parent->child[node->isLeft() ? dir::LEFT : dir::RIGHT] = child;
                        child->c = color::BLACK;
                        child->parent = node->parent;
                    }
                    else
                    {
                        root = lchild ? lchild : rchild;
                        root->parent = nullptr;
                    }
                    nullRBNode(&data);
                    return;
                }

                // case 9
                if(lchild != nullptr && rchild != nullptr)
                {
                    T* successor = rchild;
                    while(successor->child[dir::LEFT] != nullptr)
                    {
                        successor = successor->child[dir::LEFT];
                    }

                    // exchange color
                    color scolor = successor->c;
                    successor->c = node->c;
                    node->c = scolor;

                    // exchange children
                    T* srchild = successor->child[dir::RIGHT];
                    successor->child[dir::LEFT] = node->child[dir::LEFT];
                    if(node->child[dir::RIGHT] == successor)
                    {
                        successor->child[dir::RIGHT] = node;
                    }
                    else
                    {
                        successor->child[dir::RIGHT] = node->child[dir::RIGHT];
                    }
                    node->child[dir::LEFT] = nullptr; // has to be nullptr
                    node->child[dir::RIGHT] = srchild;
                    if(srchild != nullptr)
                    {
                        srchild->parent = node;
                    }

                    // exchange parents
                    dir ndir = node->isLeft() ? dir::LEFT : dir::RIGHT; // backup dir
                    dir sdir = successor->isLeft() ? dir::LEFT : dir::RIGHT; // backup dir
                    T* nparent = node->parent;
                    if(successor->parent == node)
                    {
                        node->parent = successor;
                        // child is alreay set
                    }
                    else
                    {
                        node->parent = successor->parent;
                        node->parent->child[sdir] = node;
                    }
                    successor->parent = nparent;

                    // update root
                    if(root == node)
                    {
                        root = successor;
                    }
                    else
                    {
                        nparent->child[ndir] = successor;
                    }

                    // update parents of succesor child
                    successor->child[dir::LEFT]->parent = successor;
                    successor->child[dir::RIGHT]->parent = successor;

                    remove(data);
                    return; // data is already nulled
                }

                // case 10
                if(node->c == color::RED)
                {
                    node->parent->child[node->isLeft() ? dir::LEFT : dir::RIGHT] = nullptr;
                    nullRBNode(&data);
                    return;
                }

                // removal of a black non root-leaf
                {
                    T* parent = node->parent;
                    dir node_dir = node->isLeft() ? dir::LEFT : dir::RIGHT;
                    T* sibling;
                    T* closenephew;
                    T* distantnephew;

                    // remove node
                    parent->child[node_dir] = nullptr;

                    // fix the tree
                    do
                    {
                        if(parent->child[node_dir] != nullptr)
                            node_dir = node->isLeft() ? dir::LEFT : dir::RIGHT;

                        sibling = parent->child[1 - node_dir];
                        distantnephew = sibling->child[1 - node_dir];
                        closenephew = sibling->child[node_dir];

                        // check case 3
                        if(sibling->c == color::RED)
                        {
                            // case 3
                            rotate(parent, node_dir);
                            parent->c = color::RED;
                            sibling->c = color::BLACK;
                            sibling = closenephew;

                            distantnephew = sibling->child[1 - node_dir];
                            closenephew = sibling->child[node_dir];
                        }

                        // check case 6
                        if(distantnephew != nullptr && distantnephew->c == color::RED)
                        {
                            // case 6
                            rotate(parent, node_dir);
                            sibling->c = parent->c;
                            parent->c = color::BLACK;
                            distantnephew->c = color::BLACK;
                            nullRBNode(&data);
                            return;
                        }

                        // check case 5
                        if(closenephew != nullptr && closenephew->c == color::RED)
                        {
                            // case 5
                            rotate(sibling, (dir)(1 - node_dir));
                            sibling->c = color::RED;
                            closenephew->c = color::BLACK;
                            distantnephew = sibling;
                            sibling = closenephew;
                            
                            // case 6 (again)
                            rotate(parent, node_dir);
                            sibling->c = parent->c;
                            parent->c = color::BLACK;
                            distantnephew->c = color::BLACK;
                            nullRBNode(&data);
                            return;
                        }

                        // check case 4
                        if(parent->c == color::RED)
                        {
                            // case 4
                            sibling->c = color::RED;
                            parent->c = color::BLACK;
                            nullRBNode(&data);
                            return;
                        }

                        // case 2
                        sibling->c = color::RED;
                        node = parent;
                    }
                    while((parent = node->parent) != nullptr);

                    // case 1
                    nullRBNode(&data);
                    return;
                }
            }

            /**
             * @brief Returns the minimum element of the tree by binary search
             * 
             * @return T* the minimum element of the tree
             */
            T* getMin() const
            {
                if(isEmpty())
                    return nullptr;

                T *node = root;
                while(node->child[dir::LEFT] != nullptr)
                {
                    node = node->child[dir::LEFT];
                }
                return node;
            }

            /**
             * @brief Returns the cached minimum element of the tree
             * 
             * @return T* the minimum element of the tree
             */
            T* getMin_cached() const
            {
                return min;
            }
    };
}

#endif // RBTREE_H

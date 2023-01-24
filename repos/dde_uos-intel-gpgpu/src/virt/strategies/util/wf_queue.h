#ifndef  WF_QUEUE
#define  WF_QUEUE

namespace gpgpu_virt::util {

    /**
     * @brief Wait-Free Queue
     * http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
     */
    class WFQueue
    {

    public:
        class Chain
        {
        private:
            Chain(const Chain &copy);

        public:
            Chain() : next(nullptr) {}
            Chain* volatile next;
        };

    private:
        WFQueue(const WFQueue& copy);
        Chain* volatile head;

    protected:
        Chain* tail;
        Chain stub;

    public:
        /**
         * @brief Construct a new WFQueue
         * 
         */
        WFQueue() : head(&stub), tail(&stub), stub() { }

        /**
         * @brief 
         * 
         * @param item 
         */
        inline void enqueue(Chain* item);

        /**
         * @brief 
         * 
         * @return Chain* 
         */
        inline Chain* dequeue();
        
        /**
         * @brief 
         * 
         * @param item 
         */
        inline void remove(Chain* item);
        
        /**
         * @brief 
         * 
         * @return true 
         * @return false 
         */
        inline bool empty() { return tail->next == nullptr; }
    };

    inline void WFQueue::enqueue(Chain* item)
    {
        item->next = nullptr;
        Chain* prev = __sync_lock_test_and_set(&head, item);
        prev->next = item;
    }

    inline WFQueue::Chain* WFQueue::dequeue()
    {
        Chain* t = (Chain*)tail;
        Chain* n = t->next;
        if (t == &stub) {
            if (nullptr == n) return nullptr;
            tail = n;
            t = n;
            n = n->next;
        }
        if (n) {
            tail = n;
            t->next = nullptr;
            return t;
        }
        volatile Chain* h = head;
        if (t != h) return nullptr;
        enqueue(&stub);
        n = t->next;
        if (n) {
            tail = n;
            t->next = nullptr;
            return t;
        }
        return nullptr;
    }

    inline void WFQueue::remove(Chain* item)
    {
        // serach item
        Chain* prev = head;
        while(prev->next != item && prev->next != nullptr)
        {
            prev = prev->next;
        }

        // remove it
        if(prev != nullptr)
        {
            prev->next = item->next;
            item->next = nullptr;
        }
    }

}

#endif  // WF_QUEUE

#pragma once

#include "task.h"
#include <cstdint>
#include <mx/queue/bound_mpmc.h>
#include <mx/queue/priority_queue.h>

namespace mx::tasking {
template <std::size_t CAPACITY> class SharedTaskQueue
{
public:
    SharedTaskQueue() : _queue(CAPACITY) {}
    ~SharedTaskQueue() = default;

    void push_back(TaskInterface *task) noexcept { _queue.try_push_back(task); }

    [[nodiscard]] TaskInterface *pop_front() noexcept
    {
        TaskInterface *task = nullptr;
        _queue.try_pop_front(task);
        return task;
    }

    [[nodiscard]] bool empty() const noexcept { return _queue.empty(); }

private:
    queue::BoundMPMC<TaskInterface *> _queue;
};

using GlobalSharedTaskQueue = queue::PriorityQueue<SharedTaskQueue<1U << 22U>, priority::low, priority::normal>;
using NUMASharedTaskQueue = queue::PriorityQueue<SharedTaskQueue<1U << 20U>, priority::low, priority::normal>;
} // namespace mx::tasking
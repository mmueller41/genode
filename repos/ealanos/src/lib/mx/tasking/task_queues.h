#pragma once

#include "config.h"
#include "task.h"
#include "task_buffer.h"
#include <mx/memory/config.h>
#include <mx/queue/list.h>
#include <mx/queue/mpsc.h>
#include <mx/queue/priority_queue.h>

namespace mx::tasking {
template <config::queue_backend M> class TaskQueues
{
};

template <> class TaskQueues<config::queue_backend::Single>
{
public:
    TaskQueues(const std::uint16_t /*worker_id*/, const std::uint8_t /*numa_node_id*/,
               const std::uint16_t /*count_workers*/)
    {
    }
    ~TaskQueues() = default;

    void push_back_remote(TaskInterface *task, const std::uint8_t /*numa_node_id*/,
                          const std::uint16_t /*local_worker_id*/) noexcept
    {
        _queue.get(task->annotation().priority()).push_back(task);
    }

    void push_back_local(TaskInterface *task) noexcept { _queue.get(task->annotation().priority()).push_back(task); }

    void push_back_local(TaskInterface *first, TaskInterface *last) noexcept
    {
        _queue.get(first->annotation().priority()).push_back(first, last);
    }

    template <priority P>
    [[nodiscard]] std::uint64_t fill(TaskBuffer<config::task_buffer_size()> &task_buffer,
                                     std::uint64_t available) noexcept
    {
        available -= task_buffer.template fill(_queue.template get<P>(), available);
        return task_buffer.max_size() - available;
    }

private:
    using MPSC = queue::PriorityQueue<queue::MPSC<TaskInterface>, priority::low, priority::normal>;

    /// Single queue per worker.
    MPSC _queue;
};

template <> class TaskQueues<config::queue_backend::NUMALocal>
{
public:
    TaskQueues(const std::uint16_t /*worker_id*/, const std::uint8_t numa_node_id,
               const std::uint16_t /*count_workers*/)
        : _numa_node_id(numa_node_id)
    {
    }
    ~TaskQueues() = default;

    void push_back_remote(TaskInterface *task, const std::uint8_t numa_node_id,
                          const std::uint16_t /*local_worker_id*/) noexcept
    {
        _remote_queues[numa_node_id].get(task->annotation().priority()).push_back(task);
    }

    void push_back_local(TaskInterface *task) noexcept
    {
        _local_queue.get(task->annotation().priority()).push_back(task);
    }

    void push_back_local(TaskInterface *first, TaskInterface *last) noexcept
    {
        _local_queue.get(first->annotation().priority()).push_back(first, last);
    }

    template <priority P>
    [[nodiscard]] std::uint64_t fill(TaskBuffer<config::task_buffer_size()> &task_buffer,
                                     std::uint64_t available) noexcept
    {
        // 1) Fill up from the local queue.
        available -= task_buffer.fill(_local_queue.get<P>(), available);

        if (available > 0U)
        {
            // 2) Fill up from remote queues; start with the NUMA-local one.
            for (auto numa_index = 0U; numa_index < memory::config::max_numa_nodes(); ++numa_index)
            {
                static_assert((memory::config::max_numa_nodes() & (memory::config::max_numa_nodes() - 1U)) == 0U);

                const auto numa_id = (_numa_node_id + numa_index) & (memory::config::max_numa_nodes() - 1U);
                available -= task_buffer.fill(_remote_queues[numa_id].get<P>(), available);
            }
        }

        return task_buffer.max_size() - available;
    }

private:
    using List = queue::PriorityQueue<queue::List<TaskInterface>, priority::low, priority::normal>;
    using MPSC = queue::PriorityQueue<queue::MPSC<TaskInterface>, priority::low, priority::normal>;

    const std::uint8_t _numa_node_id;

    // Backend queues for a single producer (owning worker thread) and different priorities.
    List _local_queue;

    // Backend queues for multiple produces in different NUMA regions and different priorities,
    alignas(64) std::array<MPSC, memory::config::max_numa_nodes()> _remote_queues;
};

template <> class TaskQueues<config::queue_backend::WorkerLocal>
{
public:
    TaskQueues(const std::uint16_t worker_id, const std::uint8_t /*numa_node_id*/, const std::uint16_t count_workers)
        : _worker_id(worker_id), _count_workers(count_workers)
    {
    }
    ~TaskQueues() = default;

    void push_back_remote(TaskInterface *task, const std::uint8_t /*numa_node_id*/,
                          const std::uint16_t local_worker_id) noexcept
    {
        _queues[local_worker_id].get(task->annotation().priority()).push_back(task);
    }

    void push_back_local(TaskInterface *task) noexcept
    {
        _queues[_worker_id].get(task->annotation().priority()).push_back(task);
    }

    void push_back_local(TaskInterface *first, TaskInterface *last) noexcept
    {
        _queues[_worker_id].get(first->annotation().priority()).push_back(first, last);
    }

    template <priority P>
    [[nodiscard]] std::uint64_t fill(TaskBuffer<config::task_buffer_size()> &task_buffer,
                                     std::uint64_t available) noexcept
    {
        auto worker_id = _worker_id;
        for (auto i = 0U; i < _count_workers; ++i)
        {
            const auto target_worker_id = (worker_id + i) % _count_workers;
            available -= task_buffer.fill(_queues[target_worker_id].get<P>(), available);

            if (available == 0U)
            {
                return task_buffer.max_size();
            }
        }

        return task_buffer.max_size() - available;
    }

private:
    using MPSC = queue::PriorityQueue<queue::MPSC<TaskInterface>, priority::low, priority::normal>;

    const std::uint16_t _worker_id;
    const std::uint16_t _count_workers;

    // One queue per worker.
    std::array<MPSC, config::max_cores()> _queues;
};

} // namespace mx::tasking
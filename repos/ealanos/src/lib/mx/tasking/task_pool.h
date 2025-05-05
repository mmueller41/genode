#pragma once

#include "config.h"
#include "task.h"
#include "task_buffer.h"
#include "task_pool_occupancy.h"
#include "task_queues.h"
#include <array>
#include <mx/memory/config.h>
#include <mx/queue/list.h>
#include <mx/queue/mpsc.h>
#include <mx/queue/priority_queue.h>

namespace mx::tasking {
class alignas(64) TaskPool
{
public:
    explicit TaskPool(const std::uint16_t count_workers, const std::uint16_t worker_id, const std::uint8_t numa_id)
        : _queues(worker_id, numa_id, count_workers)
    {
    }

    [[nodiscard]] std::uint64_t withdraw(TaskBuffer<config::task_buffer_size()> &task_buffer) noexcept
    {
        // Fill with normal prioritized.
        const auto size = _queues.fill<priority::normal>(task_buffer, task_buffer.available_slots());

        // Fill with low prioritized.
        if (task_buffer.empty()) [[unlikely]]
        {
            return _queues.fill<priority::low>(task_buffer, config::task_buffer_size());
        }

        return size;
    }

    /**
     * Schedules the task to thread-safe queue with regard to the NUMA region
     * of the producer. Producer of different NUMA regions should not share
     * a single queue.
     * @param task Task to be scheduled.
     * @param local_numa_node_id NUMA region of the producer.
     * @param local_worker_id Worker ID of the producer.
     */
    void push_back_remote(TaskInterface *task, const std::uint8_t local_numa_node_id,
                          const std::uint16_t local_worker_id) noexcept
    {
        _queues.push_back_remote(task, local_numa_node_id, local_worker_id);
    }

    /**
     * Schedules a task to the local queue, which is not thread-safe. Only
     * the channel owner should spawn tasks this way.
     * @param task Task to be scheduled.
     */
    void push_back_local(TaskInterface *task) noexcept { _queues.push_back_local(task); }

    /**
     * Schedules a task to the local queue, which is not thread-safe. Only
     * the channel owner should spawn tasks this way.
     * @param first First task to be scheduled.
     * @param last Last task of the list.
     */
    void push_back_local(TaskInterface *first, TaskInterface *last) noexcept { _queues.push_back_local(first, last); }

    /**
     * Adds usage prediction of a resource to this channel.
     * @param usage Predicted usage.
     */
    void predict_usage(const mx::resource::expected_access_frequency usage) noexcept { _occupancy.predict(usage); }

    /**
     * Updates the usage prediction of this channel.
     * @param old_prediction So far predicted usage.
     * @param new_prediction New predicted usage.
     */
    void modify_predicted_usage(const mx::resource::expected_access_frequency old_prediction,
                                const mx::resource::expected_access_frequency new_prediction) noexcept
    {
        _occupancy.revoke(old_prediction);
        _occupancy.predict(new_prediction);
    }

    /**
     * @return Aggregated predicted usage.
     */
    [[nodiscard]] mx::resource::expected_access_frequency predicted_usage() const noexcept
    {
        return static_cast<mx::resource::expected_access_frequency>(_occupancy);
    }

    /**
     * @return True, whenever min. one prediction was "excessive".
     */
    [[nodiscard]] bool has_excessive_usage_prediction() const noexcept
    {
        return _occupancy.has_excessive_usage_prediction();
    }

private:
    /// Backend queues.
    TaskQueues<config::queue()> _queues;

    // Holder of resource predictions of this channel.
    alignas(64) TaskPoolOccupancy _occupancy;
};
} // namespace mx::tasking
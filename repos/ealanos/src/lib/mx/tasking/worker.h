#pragma once

#include "config.h"
#include "load.h"
#include "prefetch_distance.h"
#include "profiling/task_counter.h"
#include "profiling/task_tracer.h"
#include "task.h"
#include "task_buffer.h"
#include "task_pool.h"
#include "task_pool_occupancy.h"
#include "task_stack.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mx/memory/reclamation/epoch_manager.h>
#include <mx/util/maybe_atomic.h>
#include <optional>
#include <variant>
#include <vector>

namespace mx::tasking {
/**
 * The worker executes tasks from his own channel, until the "running" flag is false.
 */
class alignas(64) Worker
{
public:
    Worker(std::uint16_t count_workers, std::uint16_t worker_id, std::uint16_t target_core_id,
           const util::maybe_atomic<bool> &is_running, PrefetchDistance prefetch_distance,
           memory::reclamation::LocalEpoch &local_epoch, const std::atomic<memory::reclamation::epoch_t> &global_epoch,
           std::optional<profiling::TaskCounter> &statistic,
           std::optional<profiling::TaskTracer> &task_tracer) noexcept;

    ~Worker() = default;

    /**
     * Starts the worker (typically in its own thread).
     */
    void execute();

    /**
     * @return Id of the logical core this worker runs on.
     */
    [[nodiscard]] std::uint16_t core_id() const noexcept { return _target_core_id; }

    [[nodiscard]] TaskPool &queues() noexcept { return _task_pool; }

    [[nodiscard]] float load() const noexcept { return _load.get(); }

    [[nodiscard]] const TaskPoolOccupancy &occupancy() const noexcept { return _occupancy; }
    [[nodiscard]] TaskPoolOccupancy &occupancy() noexcept { return _occupancy; }

private:
    // Id of the worker.
    const std::uint16_t _id;

    // Id of the logical core.
    const std::uint16_t _target_core_id;

    // Task buffer with tasks pulled out from task pools.
    TaskBuffer<config::task_buffer_size()> _task_buffer;

    // Queues where tasks are dispatched to.
    TaskPool _task_pool;

    alignas(64) Load _load;

    // Stack for persisting tasks in optimistic execution. Optimistically
    // executed tasks may fail and be restored after execution.
    alignas(64) TaskStack _task_backup_stack;

    alignas(64) TaskPoolOccupancy _occupancy;

    // Local epoch of this worker.
    memory::reclamation::LocalEpoch &_local_epoch;

    // Global epoch.
    const std::atomic<memory::reclamation::epoch_t> &_global_epoch;

    // Task counter if counting is enabled.
    std::optional<profiling::TaskCounter> &_task_counter;

    // Task tracer if task tracing is enabled.
    std::optional<profiling::TaskTracer> &_task_tracer;

    // Flag for "running" state of MxTasking.
    const util::maybe_atomic<bool> &_is_running;

    /**
     * Analyzes the given task and chooses the execution method regarding synchronization.
     * @param task Task to be executed.
     * @return Synchronization method.
     */
    static synchronization::primitive synchronization_primitive(TaskInterface *task) noexcept
    {
        return task->annotation().has_resource() ? task->annotation().resource().synchronization_primitive()
                                                 : synchronization::primitive::None;
    }

    /**
     * Executes a task with a latch.
     * @param worker_id Id of the core.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_exclusive_latched(std::uint16_t worker_id, TaskInterface *task);

    /**
     * Executes a task with a reader/writer latch.
     * @param worker_id Id of the core.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_reader_writer_latched(std::uint16_t worker_id, TaskInterface *task);

    /**
     * Executes a task with restricted transactional memory.
     * @param worker_id Id of the core.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_transactional(std::uint16_t worker_id, TaskInterface *task);

    /**
     * Executes the task optimistically.
     * @param worker_id Id of the core.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_optimistic(std::uint16_t worker_id, TaskInterface *task);

    /**
     * Executes the task using olfit protocol.
     * @param worker_id Id of the core.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_olfit(std::uint16_t worker_id, TaskInterface *task);

    /**
     * Executes the read-only task optimistically.
     * @param worker_id Id of the core.
     * @param resource Resource the task reads.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_optimistic_read(std::uint16_t worker_id, mx::resource::ResourceInterface *resource,
                                       TaskInterface *task);
};
} // namespace mx::tasking
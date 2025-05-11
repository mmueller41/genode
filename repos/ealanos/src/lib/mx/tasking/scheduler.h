#pragma once
#include "prefetch_distance.h"
#include "shared_task_queue.h"
#include "task.h"
#include "task_squad.h"
#include "worker.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mx/memory/config.h>
#include <mx/memory/reclamation/epoch_manager.h>
#include <mx/memory/worker_local_dynamic_size_allocator.h>
#include <mx/resource/ptr.h>
#include <mx/tasking/profiling/idle_profiler.h>
#include <mx/tasking/profiling/task_counter.h>
#include <mx/tasking/profiling/task_tracer.h>
#include <mx/util/core_set.h>
#include <mx/util/random.h>
#include <optional>
#include <string>

namespace mx::tasking {
/**
 * The scheduler is the central (but hidden by the runtime) data structure to spawn
 * tasks between worker threads.
 */
class Scheduler
{
public:
    Scheduler(const util::core_set &core_set, PrefetchDistance prefetch_distance,
              memory::dynamic::local::Allocator &resource_allocator) noexcept;
    ~Scheduler() noexcept;

    /**
     * Schedules a given task.
     *
     * @param task Task to be scheduled.
     * @param local_worker_id Worker, the request came from.
     * @return Worker ID where the task was dispatched to.
     */
    std::uint16_t dispatch(TaskInterface &task, std::uint16_t local_worker_id) noexcept;

    /**
     * Schedules a given list of tasks to the local worker.
     * The tasks have to be concatenated.
     *
     * @param first First task of the list.
     * @param last Last task of the list.
     * @param local_worker_id Worker, the request came from.
     * @return Worker ID where the task was dispatched to.
     */
    std::uint16_t dispatch(TaskInterface &first, TaskInterface &last, std::uint16_t local_worker_id) noexcept;

    /**
     * Schedules all tasks of a given squad.
     * @param squad Squad to be scheduled.
     * @param local_worker_id Channel, the request came from.
     * @return Worker ID where the task was dispatched to.
     */
    std::uint16_t dispatch(mx::resource::ptr squad, enum Annotation::resource_boundness boundness,
                           std::uint16_t local_worker_id) noexcept;

    /**
     * Starts all worker threads and waits until they finish.
     */
    void start_and_wait();

    /**
     * Interrupts the worker threads. They will finish after executing
     * their current tasks.
     */
    void interrupt() noexcept
    {
        _is_running = false;
        if (this->_idle_profiler.is_running())
        {
            this->_idle_profiler.stop();
        }
    }

    /**
     * @return Core set of this instance.
     */
    [[nodiscard]] const util::core_set &core_set() const noexcept { return _core_set; }

    /**
     * @return True, when the worker threads are not interrupted.
     */
    [[nodiscard]] bool is_running() const noexcept { return _is_running; }

    /**
     * @return The global epoch manager.
     */
    [[nodiscard]] memory::reclamation::EpochManager &epoch_manager() noexcept { return _epoch_manager; }

    /**
     * @return Number of all cores.
     */
    [[nodiscard]] std::uint16_t count_cores() const noexcept { return _core_set.count_cores(); }

    /**
     * @return Number of all numa regions.
     */
    [[nodiscard]] std::uint8_t count_numa_nodes() const noexcept { return _core_set.numa_nodes(); }

    /**
     * @return Prefetch distance.
     */
    [[nodiscard]] PrefetchDistance prefetch_distance() const noexcept { return _prefetch_distance; }

    /**
     * Reads the NUMA region of a given worker thread.
     * @param worker_id Worker.
     * @return NUMA region of the given worker.
     */
    [[nodiscard]] std::uint8_t numa_node_id(const std::uint16_t worker_id) const noexcept
    {
        return _worker_numa_node_map[worker_id];
    }

    /**
     * Predicts usage for a given channel.
     * @param worker_id Worker.
     * @param usage Usage to predict.
     */
    void predict_usage(const std::uint16_t worker_id, const mx::resource::expected_access_frequency usage) noexcept
    {
        _worker[worker_id]->occupancy().predict(usage);
    }

    /**
     * Updates the predicted usage of a channel.
     * @param worker_id Worker.
     * @param old_prediction So far predicted usage.
     * @param new_prediction New prediction.
     */
    void modify_predicted_usage(const std::uint16_t worker_id,
                                const mx::resource::expected_access_frequency old_prediction,
                                const mx::resource::expected_access_frequency new_prediction) noexcept
    {
        _worker[worker_id]->occupancy().revoke(old_prediction);
        _worker[worker_id]->occupancy().predict(new_prediction);
    }

    /**
     * @param worker_id Worker.
     * @return True, when a least one usage was predicted to be "excessive" for the given channel.
     */
    [[nodiscard]] bool has_excessive_usage_prediction(const std::uint16_t worker_id) const noexcept
    {
        return _worker[worker_id]->occupancy().has_excessive_usage_prediction();
    }

    /**
     * Resets the statistics.
     */
    void reset() noexcept;

    /**
     * Starts profiling of idle times.
     */
    void start_idle_profiler();

    /**
     * Stops idle profiling.
     * @return List of idle times for each channel.
     */
    [[nodiscard]] profiling::IdleTimes stop_idle_profiler() { return this->_idle_profiler.stop(); }

    /**
     * @return Statistic.
     */
    [[nodiscard]] std::optional<profiling::TaskCounter> &task_counter() noexcept { return _task_counter; }

    /**
     * @return Task tracer.
     */
    [[nodiscard]] std::optional<profiling::TaskTracer> &task_tracer() noexcept { return _task_tracer; }

    [[nodiscard]] std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>> memory_tags();

    bool operator==(const util::core_set &cores) const noexcept { return _core_set == cores; }

    bool operator!=(const util::core_set &cores) const noexcept { return _core_set != cores; }

private:
    class PhysicalCoreResourceWorkerIds
    {
    public:
        PhysicalCoreResourceWorkerIds() noexcept
            : _worker_ids{std::numeric_limits<std::uint16_t>::max(), std::numeric_limits<std::uint16_t>::max(),
                          std::numeric_limits<std::uint16_t>::max()}
        {
        }

        explicit PhysicalCoreResourceWorkerIds(const std::uint16_t worker_id) noexcept
            : _worker_ids{worker_id, worker_id, worker_id}
        {
        }

        PhysicalCoreResourceWorkerIds(const std::uint16_t memory_bound_worker_id,
                                      const std::uint16_t compute_bound_worker_id,
                                      const std::uint16_t mixed_worker_id) noexcept
            : _worker_ids{memory_bound_worker_id, compute_bound_worker_id, mixed_worker_id}
        {
        }

        ~PhysicalCoreResourceWorkerIds() noexcept = default;

        PhysicalCoreResourceWorkerIds &operator=(const PhysicalCoreResourceWorkerIds &) noexcept = default;
        PhysicalCoreResourceWorkerIds &operator=(PhysicalCoreResourceWorkerIds &&) noexcept = default;

        [[nodiscard]] std::uint16_t operator[](const enum Annotation::resource_boundness boundness) const noexcept
        {
            return _worker_ids[boundness];
        }

        [[nodiscard]] std::uint16_t &operator[](const enum Annotation::resource_boundness boundness) noexcept
        {
            return _worker_ids[boundness];
        }

        operator bool()
        {
            return _worker_ids[0U] != std::numeric_limits<std::uint16_t>::max() &&
                   _worker_ids[1U] != std::numeric_limits<std::uint16_t>::max() &&
                   _worker_ids[2U] != std::numeric_limits<std::uint16_t>::max();
        }

    private:
        std::array<std::uint16_t, 3U> _worker_ids;
    };

    // Cores to run the worker threads on.
    const util::core_set _core_set;

    // Number of tasks a resource will be prefetched in front of.
    const PrefetchDistance _prefetch_distance;

    // All initialized workers.
    std::array<Worker *, config::max_cores()> _worker{nullptr};

    // Map of worker id to NUMA region id.
    std::array<std::uint8_t, config::max_cores()> _worker_numa_node_map{0U};

    // Flag for the worker threads. If false, the worker threads will stop.
    // This is atomic for hardware that does not guarantee atomic reads/writes of booleans.
    alignas(64) util::maybe_atomic<bool> _is_running{false};

    // Epoch manager for memory reclamation,
    alignas(64) memory::reclamation::EpochManager _epoch_manager;

    // Profiler for task statistics.
    alignas(64) std::optional<profiling::TaskCounter> _task_counter{std::nullopt};

    // Profiler for idle times.
    alignas(64) profiling::IdleProfiler _idle_profiler;

    // Recorder for tracing task run times.
    alignas(64) std::optional<profiling::TaskTracer> _task_tracer{std::nullopt};

    /**
     * Make a decision whether a task should be scheduled to the local
     * channel or a remote.
     *
     * @param is_readonly Access mode of the task.
     * @param primitive The synchronization primitive of the task annotated resource.
     * @param resource_worker_id Worker id of the task annotated resource.
     * @param current_worker_id Worker id where the spawn() operation is called.
     * @return True, if the task should be scheduled local.
     */
    [[nodiscard]] static inline bool keep_task_local(const bool is_readonly, const synchronization::primitive primitive)
    {
        return (is_readonly && primitive != synchronization::primitive::ScheduleAll) ||
               (primitive != synchronization::primitive::None && primitive != synchronization::primitive::ScheduleAll &&
                primitive != synchronization::primitive::ScheduleWriter);
    }
};
} // namespace mx::tasking
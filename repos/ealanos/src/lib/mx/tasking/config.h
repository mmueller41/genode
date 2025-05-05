#pragma once

namespace mx::tasking {
class config
{
public:
    enum queue_backend
    {
        Single,     /// Each worker has a single queue.
        NUMALocal,  /// Each worker has a queue for each NUMA domain and a local queue.
        WorkerLocal /// Each worker has a queue for each worker.
    };

    enum memory_reclamation_scheme
    {
        None = 0U,                   /// No memory reclamation at all.
        UpdateEpochOnRead = 1U,      /// End the epoch after every reading task.
        UpdateEpochPeriodically = 2U /// End the epoch after a static amount of time.
    };

    enum worker_mode
    {
        Performance = 0U, /// The worker contact the task pool when no task was found.
        PowerSave = 1U    /// The worker will sleep a static amount of time when no task was found.
    };

    /// Maximal number of supported cores.
    static constexpr auto max_cores() { return 64U; }

    /// Backend of the queues.
    static constexpr auto queue() { return queue_backend::NUMALocal; }

    /// Maximal number of supported simultaneous multithreading threads.
    static constexpr auto max_smt_threads() { return 2U; }

    /// Maximal size for a single task, will be used for task allocation.
    static constexpr auto task_size() { return 128U; }

    /// The task buffer will hold a set of tasks, fetched from
    /// queues. This is the size of the buffer.
    static constexpr auto task_buffer_size() { return 64U; }

    /// If enabled, the worker will sample task cycles during execution
    /// and use that stats for approximating the prefetch distance for each
    /// task.
    /// If disabled, automatic prefetching will fall back to task annotations.
    static constexpr auto is_monitor_task_cycles_for_prefetching() { return false; }

    /// If enabled, will record the number of execute tasks,
    /// scheduled tasks, reader and writer per core and more.
    static constexpr auto is_use_task_counter() { return false; }

    /// If enabled, the runtime of each task will be recorded.
    static constexpr auto is_collect_task_traces() { return false; }

    /// If enabled, the dataflow graph will collect statistics which node
    /// emitted which amount of data.
    static constexpr auto is_count_graph_emits() { return false; }

    /// If enabled, the dataflow graph will collect start times of
    /// pipelines and finish times of nodes.
    static constexpr auto is_record_graph_times() { return false; }

    /// If enabled, memory will be reclaimed while using optimistic
    /// synchronization by epoch-based reclamation. Otherwise, freeing
    /// memory is unsafe.
    static constexpr auto memory_reclamation() { return memory_reclamation_scheme::None; }

    /// Switch between performance and power saving mode.
    /// Set to 'worker_mode::Performance' for measurements.
    static constexpr auto worker_mode() { return worker_mode::Performance; }
};
} // namespace mx::tasking

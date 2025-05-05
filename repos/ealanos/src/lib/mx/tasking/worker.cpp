#include "worker.h"
#include "config.h"
#include "runtime.h"
#include "task.h"
#include <cassert>
#include <mx/system/builtin.h>
#include <mx/system/cpu.h>
#include <mx/system/rdtscp.h>

using namespace mx::tasking;

Worker::Worker(const std::uint16_t count_workers, const std::uint16_t worker_id, const std::uint16_t target_core_id,
               const util::maybe_atomic<bool> &is_running, const PrefetchDistance prefetch_distance,
               memory::reclamation::LocalEpoch &local_epoch,
               const std::atomic<memory::reclamation::epoch_t> &global_epoch,
               std::optional<profiling::TaskCounter> &statistic,
               std::optional<profiling::TaskTracer> &task_tracer) noexcept
    : _id(worker_id), _target_core_id(target_core_id), _task_buffer(prefetch_distance),
      _task_pool(count_workers, worker_id, system::cpu::node_id(target_core_id)), _local_epoch(local_epoch),
      _global_epoch(global_epoch), _task_counter(statistic), _task_tracer(task_tracer), _is_running(is_running)
{
}

void Worker::execute()
{
    runtime::initialize_worker(this->_id);

    auto trace_start = std::chrono::system_clock::now();

    const auto refill_treshold = this->_task_buffer.refill_treshold();

    while (this->_is_running == false)
    {
        system::builtin::pause();
    }

    assert(this->_target_core_id == system::cpu::core_id() && "Worker not pinned to correct core.");
    const auto worker_id = this->_id;

    /// Period the task sampler for monitoring task cycles becomes active.
    constexpr auto sample_period = 4096U;

    /// Store frequently used pool and buffer on the stack.
    auto &pool = this->_task_pool;
    auto &buffer = this->_task_buffer;
    auto &sampler = buffer.sampler();

    const auto is_prefetching_enabled = this->_task_buffer.is_prefetching_enabled();
    auto task_counter = 0U;

    while (this->_is_running)
    {
        if constexpr (config::memory_reclamation() == config::UpdateEpochPeriodically)
        {
            this->_local_epoch.enter(this->_global_epoch);
        }

        /// Fill the task buffer with tasks.
        auto task_buffer_size = pool.withdraw(buffer);
        if constexpr (config::is_use_task_counter())
        {
            this->_task_counter->increment<profiling::TaskCounter::FilledBuffer>(worker_id);
        }

        if (task_buffer_size == 0U) [[unlikely]]
        {
            do
            {
                mx::system::builtin::pause();

                task_buffer_size = pool.withdraw(buffer);
                if constexpr (config::is_use_task_counter())
                {
                    this->_task_counter->increment<profiling::TaskCounter::FilledBuffer>(worker_id);
                }
            } while (task_buffer_size == 0U && this->_is_running);
        }

        /// Enter epoch when increased periodically.
        if constexpr (config::memory_reclamation() == config::UpdateEpochPeriodically)
        {
            this->_local_epoch.enter(this->_global_epoch);
        }

        const auto count_available_tasks =
            task_counter + std::max(std::int32_t(task_buffer_size) - refill_treshold,
                                    std::min(std::int32_t(task_buffer_size), std::int32_t(refill_treshold)));

        auto task_trace_id = std::uint64_t(0U);
        auto is_sampling = false;
        std::uint64_t sample_cycles;

        for (; task_counter < count_available_tasks; ++task_counter)
        {
            if constexpr (config::is_monitor_task_cycles_for_prefetching())
            {
                is_sampling = (task_counter & (sample_period - 1U)) == 0U;
            }

            /// Get the next slot with task and prefetch hint.
            auto &slot = buffer.next();

            /// Get the task to execute.
            auto *task = slot.get();

            /// Prefetching
            if (is_prefetching_enabled)
            {
                /// The task executed next and
                auto *task_descriptor = buffer.head();
                system::cache::prefetch_range<system::cache::level::ALL, system::cache::access::read,
                                              config::task_size()>(reinterpret_cast<std::int64_t *>(task_descriptor));

                /// the data placed in the prefetching buffer
                slot.prefetch();
            }

            /// Increase task execution counter.
            if constexpr (config::is_use_task_counter())
            {
                this->_task_counter->increment<profiling::TaskCounter::Executed>(worker_id);
                if (task->annotation().has_resource())
                {
                    if (task->annotation().is_readonly())
                    {
                        this->_task_counter->increment<profiling::TaskCounter::ExecutedReader>(worker_id);
                    }
                    else
                    {
                        this->_task_counter->increment<profiling::TaskCounter::ExecutedWriter>(worker_id);
                    }
                }
            }

            /// Collect task times, when tracing.
            if constexpr (config::is_collect_task_traces())
            {
                task_trace_id = task->trace_id();
                trace_start = std::chrono::system_clock::now();
            }

            /// Sample the task, if monitoring.
            if constexpr (config::is_monitor_task_cycles_for_prefetching())
            {
                if (is_sampling)
                {
                    task_trace_id = task->trace_id();
                    sample_cycles = system::RDTSCP::begin();
                }
            }

            // Based on the annotated resource and its synchronization
            // primitive, we choose the fitting execution context.
            auto result = TaskResult{};
            switch (Worker::synchronization_primitive(task))
            {
            case synchronization::primitive::ScheduleWriter:
                result = this->execute_optimistic(worker_id, task);
                break;
            case synchronization::primitive::OLFIT:
                result = this->execute_olfit(worker_id, task);
                break;
            case synchronization::primitive::ScheduleAll:
            case synchronization::primitive::Batched:
            case synchronization::primitive::None:
                result = task->execute(worker_id);
                break;
            case synchronization::primitive::ReaderWriterLatch:
                result = Worker::execute_reader_writer_latched(worker_id, task);
                break;
            case synchronization::primitive::ExclusiveLatch:
                result = Worker::execute_exclusive_latched(worker_id, task);
                break;
            case synchronization::primitive::RestrictedTransactionalMemory:
                result = Worker::execute_transactional(worker_id, task);
                break;
            }

            if constexpr (config::is_monitor_task_cycles_for_prefetching())
            {
                if (is_sampling)
                {
                    const auto end_sample_cycles = system::RDTSCP::end();
                    sampler.add(task_trace_id, end_sample_cycles - sample_cycles);
                }
            }

            if constexpr (config::is_collect_task_traces())
            {
                const auto trace_end = std::chrono::system_clock::now();
                this->_task_tracer->emplace_back(worker_id, task_trace_id,
                                                 profiling::TimeRange({trace_start, trace_end}));
            }

            // The task-chain may be finished at time the
            // task has no successor. Otherwise, we spawn
            // the successor task.
            if (result.has_successor())
            {
                runtime::spawn(*static_cast<TaskInterface *>(result), worker_id);
            }

            /// Remove the task if requested.
            if (result.is_remove())
            {
                runtime::delete_task(worker_id, task);
            }
        }
    }

    //    if (worker_id == 0U)
    //    {
    //        std::this_thread::sleep_for(std::chrono::milliseconds(250U));
    //        for (const auto [id, sample] : sampler.get())
    //        {
    //            std::cout << runtime::task_name(id) << ": " << sample.average() << " (" << sample.cycles() << "/"
    //                      << sample.count() << ")" << std::endl;
    //        }
    //    }
}

TaskResult Worker::execute_exclusive_latched(const std::uint16_t worker_id, mx::tasking::TaskInterface *const task)
{
    auto *resource = mx::resource::ptr_cast<mx::resource::ResourceInterface>(task->annotation().resource());

    {
        auto latch = mx::resource::ResourceInterface::scoped_exclusive_latch{resource};
        return task->execute(worker_id);
    }
}

TaskResult Worker::execute_reader_writer_latched(const std::uint16_t worker_id, mx::tasking::TaskInterface *const task)
{
    auto *resource = mx::resource::ptr_cast<mx::resource::ResourceInterface>(task->annotation().resource());

    // Reader do only need to acquire a "read-only" latch.
    if (task->annotation().is_readonly())
    {
        auto reader_latch = mx::resource::ResourceInterface::scoped_rw_latch<false>{resource};
        return task->execute(worker_id);
    }

    {
        auto writer_latch = mx::resource::ResourceInterface::scoped_rw_latch<true>{resource};
        return task->execute(worker_id);
    }
}

TaskResult Worker::execute_optimistic(const std::uint16_t worker_id, mx::tasking::TaskInterface *const task)
{
    auto *optimistic_resource = task->annotation().resource().get<mx::resource::ResourceInterface>();

    if (task->annotation().is_readonly())
    {
        // For readers running at a different channel than writer,
        // we need to validate the version of the resource. This
        // comes along with saving the tasks state on a stack and
        // re-running the task, whenever the version check failed.
        if (task->annotation().resource().worker_id() != worker_id)
        {
            return this->execute_optimistic_read(worker_id, optimistic_resource, task);
        }

        // Whenever the task is executed at the same channel
        // where writing tasks are executed, we do not need to
        // synchronize because no write can happen.
        return task->execute(worker_id);
    }

    // Writers, however, need to acquire the version to tell readers, that
    // the resource is modified. This is done by making the version odd before
    // writing to the resource and even afterwards. Here, we can use a simple
    // fetch_add operation, because writers are serialized on the channel.
    {
        mx::resource::ResourceInterface::scoped_optimistic_latch writer_latch{optimistic_resource};
        return task->execute(worker_id);
    }
}

TaskResult Worker::execute_olfit(const std::uint16_t worker_id, TaskInterface *const task)
{
    auto *optimistic_resource = task->annotation().resource().get<mx::resource::ResourceInterface>();

    if (task->annotation().is_readonly())
    {
        return this->execute_optimistic_read(worker_id, optimistic_resource, task);
    }

    // Writers, however, need to acquire the version to tell readers, that
    // the resource is modified. This is done by making the version odd before
    // writing to the resource and even afterwards. Here, we need to use compare
    // xchg because writers can appear on every channel.
    {
        auto writer_latch = mx::resource::ResourceInterface::scoped_olfit_latch{optimistic_resource};
        return task->execute(worker_id);
    }
}

TaskResult Worker::execute_optimistic_read(const std::uint16_t worker_id,
                                           mx::resource::ResourceInterface *optimistic_resource,
                                           TaskInterface *const task)
{
    if constexpr (config::memory_reclamation() == config::UpdateEpochOnRead)
    {
        this->_local_epoch.enter(this->_global_epoch);
    }

    // The current state of the task is saved for
    // restoring if the read operation failed, but
    // the task was maybe modified.
    this->_task_backup_stack.backup(task);

    do
    {
        const auto version = optimistic_resource->version();
        const auto result = task->execute(worker_id);

        if (optimistic_resource->is_version_valid(version))
        {
            if constexpr (config::memory_reclamation() == config::UpdateEpochOnRead)
            {
                this->_local_epoch.leave();
            }
            return result;
        }

        if constexpr (config::is_use_task_counter())
        {
            if (task->annotation().is_readonly())
            {
                this->_task_counter->increment<profiling::TaskCounter::ExecutedReader>(worker_id);
            }
        }

        // At this point, the version check failed and we need
        // to re-run the read operation.
        this->_task_backup_stack.restore(task);
    } while (true);
}

TaskResult Worker::execute_transactional(const std::uint16_t worker_id, TaskInterface *task)
{
    auto *resource = task->annotation().resource().get<mx::resource::ResourceInterface>();
    {
        auto transaction = mx::resource::ResourceInterface::scoped_transaction{resource};
        return task->execute(worker_id);
    }
}
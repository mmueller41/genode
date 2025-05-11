#include "scheduler.h"
#include "mx/system/environment.h"
#include "mx/util/logger.h"
#include "runtime.h"
#include <mx/memory/global_heap.h>
#include <mx/synchronization/synchronization.h>
#include <mx/system/cpu.h>
#include <mx/system/thread.h>
#include <string>
#include <thread>
#include <vector>
#include <iostream>


using namespace mx::tasking;

Scheduler::Scheduler(const mx::util::core_set &core_set, const PrefetchDistance prefetch_distance,
                     memory::dynamic::local::Allocator &resource_allocator) noexcept
    : _core_set(core_set), _prefetch_distance(prefetch_distance), _worker({nullptr}),
      _epoch_manager(core_set.count_cores(), resource_allocator, _is_running)
{
    this->_worker_numa_node_map.fill(0U);

    /// Set up profiling utilities.
    if constexpr (config::is_use_task_counter())
    {
        this->_task_counter.emplace(profiling::TaskCounter{this->_core_set.count_cores()});
    }
    if constexpr (config::is_collect_task_traces() || config::is_monitor_task_cycles_for_prefetching())
    {
        this->_task_tracer.emplace(profiling::TaskTracer{this->_core_set.count_cores()});
    }

	/// Create worker.
    std::cout << "Creating workers for coreset " << this->_core_set << std::endl;
	
    for (auto worker_id = std::uint16_t(0U); worker_id < this->_core_set.count_cores(); ++worker_id)
    {
        /// The core the worker is binded to.
        const auto core_id = this->_core_set[worker_id];

        /// The corresponding NUMA Node.
        const auto numa_node_id = system::cpu::node_id(core_id);
        this->_worker_numa_node_map[worker_id] = numa_node_id;

		this->_worker[worker_id] = static_cast<Worker*>(memory::GlobalHeap::allocate(numa_node_id, sizeof(Worker)));

        std::cout << "Creating worker " << worker_id << " at " << this->_worker[worker_id];
		
		new (static_cast<void *>(this->_worker[worker_id]))
			Worker(this->_core_set.count_cores(), worker_id, core_id, this->_is_running,
		           prefetch_distance, this->_epoch_manager[worker_id],
		           this->_epoch_manager.global_epoch(), this->_task_counter, this->_task_tracer);
	    util::Logger::info_if(system::Environment::is_debug(), "Created worker threads");
    }
}

Scheduler::~Scheduler() noexcept
{
    std::for_each(this->_worker.begin(), this->_worker.begin() + this->_core_set.count_cores(), [](auto *worker) {
        const auto numa_node_id = system::cpu::node_id(worker->core_id());
        worker->~Worker();
        memory::GlobalHeap::free(worker, sizeof(Worker), numa_node_id);
    });
}

void Scheduler::start_and_wait()
{
    // Create threads for worker...
    std::vector<std::thread> worker_threads(this->_core_set.count_cores() +
                                            static_cast<std::uint16_t>(config::memory_reclamation() != config::None));
    for (auto worker_id = 0U; worker_id < this->_core_set.count_cores(); ++worker_id)
    {
        auto *worker = this->_worker[worker_id];
        worker_threads[worker_id] = std::thread([worker] { worker->execute(); });

        util::Logger::info_if(system::Environment::is_debug(), "Created worker thread " + std::to_string(worker_id) + " of size " + std::to_string(sizeof(std::thread)));
        //system::thread::pin(worker_threads[worker_id], worker->core_id());
        //system::thread::name(worker_threads[worker_id], "mx::worker#" + std::to_string(worker_id));
    }

    // ... and epoch management (if enabled).
    if constexpr (config::memory_reclamation() != config::None)
    {
        const auto memory_reclamation_thread_id = this->_core_set.count_cores();

        // In case we enable memory reclamation: Use an additional thread.
        worker_threads[memory_reclamation_thread_id] =
            std::thread([this] { this->_epoch_manager.enter_epoch_periodically(); });

        // Set name.
        //system::thread::name(worker_threads[memory_reclamation_thread_id], "mx::mem_reclam");
    }

    // Turning the flag on starts all worker threads to execute tasks.
    this->_is_running = true;

    // Wait for the worker threads to end. This will only
    // reached when the _is_running flag is set to false
    // from somewhere in the application.
    for (auto &worker_thread : worker_threads)
    {
        worker_thread.join();
    }

    if constexpr (config::memory_reclamation() != config::None)
    {
        // At this point, no task will execute on any resource;
        // but the epoch manager has joined, too. Therefore,
        // we will reclaim all memory manually.
        this->_epoch_manager.reclaim_all();
    }
}

std::uint16_t Scheduler::dispatch(TaskInterface &task, const std::uint16_t local_worker_id) noexcept
{
    /// The "local_channel_id" (the id of the calling channel) may be "invalid" (=uint16_t::max).
    /// If it is not, we set the worker_id of the worker_id either by contacting the map (virtualization on)
    /// or just using the worker_id as worker_id (virtualization off).
    const auto has_local_worker_id = local_worker_id != std::numeric_limits<std::uint16_t>::max();

    if constexpr (config::is_use_task_counter())
    {
        if (has_local_worker_id) [[likely]]
        {
            this->_task_counter->increment<profiling::TaskCounter::Dispatched>(local_worker_id);
        }
    }

    const auto &annotation = task.annotation();

    // Scheduling is based on the annotated resource of the given task.
    if (annotation.has_resource())
    {
        const auto annotated_resource = annotation.resource();
        auto resource_worker_id = annotated_resource.worker_id();

        if (annotated_resource.synchronization_primitive() == synchronization::primitive::Batched)
        {
            //            if (resource_worker_id == local_worker_id)
            //            {
            //                annotated_resource.get<TaskSquad>()->push_back_local(task);
            //                return local_worker_id;
            //            }

            annotated_resource.get<TaskSquad>()->push_back_remote(task);
            return resource_worker_id;
        }

        // For performance reasons, we prefer the local (not synchronized) queue
        // whenever possible to spawn the task. The decision is based on the
        // synchronization primitive and the access mode of the task (reader/writer).
        if (has_local_worker_id &&
            Scheduler::keep_task_local(annotation.is_readonly(), annotated_resource.synchronization_primitive()))
        {
            this->_worker[local_worker_id]->queues().push_back_local(&task);
            if constexpr (config::is_use_task_counter())
            {
                this->_task_counter->increment<profiling::TaskCounter::DispatchedLocally>(local_worker_id);
            }
            return resource_worker_id;
        }

        if (has_local_worker_id) [[likely]]
        {
            this->_worker[resource_worker_id]->queues().push_back_remote(&task, this->numa_node_id(local_worker_id),
                                                                         local_worker_id);
        }
        else
        {
            this->_worker[resource_worker_id]->queues().push_back_remote(&task, system::cpu::node_id(),
                                                                         runtime::worker_id());
        }

        if constexpr (config::is_use_task_counter())
        {
            if (has_local_worker_id) [[likely]]
            {
                this->_task_counter->increment<profiling::TaskCounter::DispatchedRemotely>(local_worker_id);
            }
        }
        return resource_worker_id;
    }

    // The developer assigned a fixed channel to the task.
    if (annotation.has_worker_id())
    {
        const auto target_worker_id = annotation.worker_id();

        if (has_local_worker_id)
        {
            // For performance reasons, we prefer the local (not synchronized) queue
            // whenever possible to spawn the task.
            if (local_worker_id == target_worker_id)
            {
                this->_worker[target_worker_id]->queues().push_back_local(&task);
                if constexpr (config::is_use_task_counter())
                {
                    this->_task_counter->increment<profiling::TaskCounter::DispatchedLocally>(target_worker_id);
                }

                return target_worker_id;
            }

            this->_worker[target_worker_id]->queues().push_back_remote(&task, this->numa_node_id(local_worker_id),
                                                                       local_worker_id);
        }
        else
        {
            this->_worker[target_worker_id]->queues().push_back_remote(&task, system::cpu::node_id(),
                                                                       runtime::worker_id());
        }

        if constexpr (config::is_use_task_counter())
        {
            if (has_local_worker_id) [[likely]]
            {
                this->_task_counter->increment<profiling::TaskCounter::DispatchedRemotely>(local_worker_id);
            }
        }

        return target_worker_id;
    }

    // The developer assigned a fixed NUMA region to the task.
    //    if (annotation.has_numa_node_id())
    //    {
    //        this->_numa_node_queues[annotation.numa_node_id()].get(annotation.priority()).push_back(&task);
    //        if constexpr (config::is_use_task_counter())
    //        {
    //            if (current_channel_id != std::numeric_limits<std::uint16_t>::max()) [[likely]]
    //            {
    //                this->_task_counter->increment<profiling::TaskCounter::DispatchedRemotely>(current_channel_id);
    //            }
    //        }
    //
    //        /// TODO: What to return?
    //        return 0U;
    //    }

    // The task should be spawned on the local channel.
    if (annotation.is_locally())
    {
        if (has_local_worker_id) [[likely]]
        {
            this->_worker[local_worker_id]->queues().push_back_local(&task);
            if constexpr (config::is_use_task_counter())
            {
                this->_task_counter->increment<profiling::TaskCounter::DispatchedLocally>(local_worker_id);
            }

            return local_worker_id;
        }

        assert(false && "Spawn was expected to be 'locally' but no local channel was provided.");
    }

    // The task can run everywhere.
    //    this->_global_queue.get(annotation.priority()).push_back(&task);
    //    if constexpr (config::is_use_task_counter())
    //    {
    //        if (current_channel_id != std::numeric_limits<std::uint16_t>::max()) [[likely]]
    //        {
    //            this->_task_counter->increment<profiling::TaskCounter::DispatchedRemotely>(current_channel_id);
    //        }
    //    }

    /// TODO: What to return?
    return 0U;
}

std::uint16_t Scheduler::dispatch(TaskInterface &first, TaskInterface &last, const uint16_t local_worker_id) noexcept
{
    this->_worker[local_worker_id]->queues().push_back_local(&first, &last);
    return local_worker_id;
}

std::uint16_t Scheduler::dispatch(const mx::resource::ptr squad, const enum Annotation::resource_boundness boundness,
                                  const std::uint16_t local_worker_id) noexcept
{
    auto *dispatch_task = runtime::new_task<TaskSquadSpawnTask>(local_worker_id, *squad.get<TaskSquad>());
    dispatch_task->annotate(squad.worker_id());
    return this->dispatch(*dispatch_task, local_worker_id);
}

void Scheduler::reset() noexcept
{
    if constexpr (config::is_use_task_counter())
    {
        this->_task_counter->clear();
    }

    this->_epoch_manager.reset();
}

void Scheduler::start_idle_profiler()
{
    // TODO: Should we measure idle times?
    //    this->_idle_profiler.start();
    //    for (auto worker_id = 0U; worker_id < this->_core_set.count_cores(); ++worker_id)
    //    {
    //        this->_idle_profiler.start(this->_worker[worker_id]->channel());
    //    }
}

std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>> Scheduler::memory_tags()
{
    auto tags = std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>{};

    auto workers = std::vector<std::pair<std::uintptr_t, std::uintptr_t>>{};
    workers.reserve(this->_core_set.count_cores());
    for (auto worker_id = 0U; worker_id < this->_core_set.count_cores(); ++worker_id)
    {
        const auto begin = std::uintptr_t(this->_worker[worker_id]);
        const auto end = begin + sizeof(Worker);
        workers.emplace_back(std::make_pair(begin, end));
    }

    tags.insert(std::make_pair("worker", std::move(workers)));
    return tags;
}
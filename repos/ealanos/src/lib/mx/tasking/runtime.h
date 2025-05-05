#pragma once
#include "ealanos/memory/hamstraaja.h"
#include "prefetch_distance.h"
#include "scheduler.h"
#include "task.h"
#include "task_squad.h"
#include <iostream>
#include <memory>
#include <mx/io/network/server.h>
#include <mx/memory/fixed_size_allocator.h>
#include <mx/memory/task_allocator_interface.h>
#include <mx/memory/worker_local_dynamic_size_allocator.h>
#include <mx/resource/annotation.h>
#include <mx/resource/builder.h>
#include <mx/system/environment.h>
#include <mx/system/thread.h>
#include <mx/util/core_set.h>
#include <mx/util/logger.h>
#include <utility>
#include <libc/component.h>

namespace mx::tasking {
/**
 * The runtime is the central access structure to MxTasking.
 * Here, we can initialize MxTasking, spawn and allocate tasks, allocate
 * data objects.
 */
class runtime
{
public:
    /**
     * Initializes the MxTasking runtime.
     *
     * @param core_set Cores, where the runtime should execute on.
     * @param prefetch_distance Distance for prefetching.
     * @param channels_per_core Number of channels per core (more than one enables channel-stealing).
     * @param use_system_allocator Should we use the systems malloc interface or our allocator?
     * @return True, when the runtime was started successfully.
     */
    static bool init(const util::core_set &core_set, const PrefetchDistance prefetch_distance,
                     const bool use_system_allocator, Libc::Env& env)
    {
        util::Logger::info_if(system::Environment::is_debug(), "Starting MxTasking in DEBUG mode.");
        util::Logger::warn_if(system::Environment::is_debug() == false && config::is_use_task_counter(),
                              "Task statistics will be collected in RELEASE build.");
        util::Logger::warn_if(system::Environment::is_debug() == false && config::is_collect_task_traces(),
                              "Task traces will be collected in RELEASE build.");
        util::Logger::warn_if(system::Environment::is_debug() == false &&
                                  config::worker_mode() == config::worker_mode::PowerSave,
                              "Power safe mode activated in RELEASE build.");

		if (!memory::GlobalHeap::initialized()) {
			memory::GlobalHeap::init(new Ealan::Memory::Hamstraaja<memory::config::min_block_size(), memory::config::superblock_cutoff()>(env.pd(), env.rm()));
		}
		
        // Are we ready to re-initialize the scheduler?
        if (_scheduler != nullptr && _scheduler->is_running())
        {
            return false;
        }

        /// The starting thread operates as worker#0.
        _worker_id = 0U;

        // Create a new resource allocator.
        if (_resource_allocator == nullptr)
        {
            /// Only called the first time.
            _resource_allocator.reset(new (memory::GlobalHeap::allocate_cache_line_aligned(
                sizeof(memory::dynamic::local::Allocator))) memory::dynamic::local::Allocator(core_set));
        }
        else if (_resource_allocator->is_free())
        {
            /// The application cleared the whole memory, but
            /// we won't re-allocate the allocator to keep references
            /// that are given to the resource builder and epoch manager
            /// since the scheduler does not change.
            _resource_allocator->reset(core_set, true);
        }
        else
        {
            /// The application holds allocated memory.
            _resource_allocator->reset(core_set, false);
        }

        // Create a new task allocator.
        if (use_system_allocator)
        {
            _task_allocator.reset(new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(
                memory::SystemTaskAllocator<config::task_size()>))) memory::SystemTaskAllocator<config::task_size()>());
        }
        else
        {
            _task_allocator.reset(new (
                memory::GlobalHeap::allocate_cache_line_aligned(sizeof(memory::fixed::Allocator<config::task_size()>)))
                                      memory::fixed::Allocator<config::task_size()>(util::core_set::build()));
        }

        // Create a new scheduler.
        const auto need_new_scheduler = _scheduler == nullptr || *_scheduler != core_set;
        if (need_new_scheduler)
        {
            _scheduler.reset(new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(Scheduler)))
                                 Scheduler(core_set, prefetch_distance, *_resource_allocator));
        }
        else
        {
            _scheduler->reset();
        }

        // Create a new resource builder.
        if (_resource_builder == nullptr || need_new_scheduler)
        {
            _resource_builder = std::make_unique<mx::resource::Builder>(*_scheduler, *_resource_allocator);
        }

        return true;
    }

    /**
     * Spawns the given task.
     *
     * @param task Task to be scheduled.
     * @param local_worker_id Worker, the spawn request came from.
     */
    static std::uint16_t spawn(TaskInterface &task, const std::uint16_t local_worker_id) noexcept
    {
        return _scheduler->dispatch(task, local_worker_id);
    }

    /**
     * Spawns the given task.
     *
     * @param task Task to be scheduled.
     */
    static void spawn(TaskInterface &task) noexcept { _scheduler->dispatch(task, _worker_id); }

    /**
     * Spawns a list of concatenated tasks.
     *
     * @param first_task First task of the list to be scheduled.
     * @param last_task Last task of the list to be scheduled.
     * @param local_worker_id Worker, the spawn request came from.
     */
    static void spawn(TaskInterface &first_task, TaskInterface &last_task, const std::uint16_t local_worker_id) noexcept
    {
        _scheduler->dispatch(first_task, last_task, local_worker_id);
    }

    /**
     * Spawns the given squad.
     *
     * @param squad Squad to be scheduled.
     * @param local_worker_id Worker, the spawn request came from.
     */
    static std::uint16_t spawn(const mx::resource::ptr squad, const std::uint16_t local_worker_id) noexcept
    {
        return spawn(squad, annotation::resource_boundness::mixed, local_worker_id);
    }

    /**
     * Spawns the given squad.
     *
     * @param squad Squad to be scheduled.
     * @param boundness Boundness of the squad.
     * @param local_worker_id Worker, the spawn request came from.
     */
    static std::uint16_t spawn(const mx::resource::ptr squad, const enum annotation::resource_boundness boundness,
                               const std::uint16_t local_worker_id) noexcept
    {
        return _scheduler->dispatch(squad, boundness, local_worker_id);
    }

    /**
     * @return Number of available cores.
     */
    [[nodiscard]] static std::uint16_t workers() noexcept { return _scheduler->count_cores(); }

    /**
     * @return Prefetch distance.
     */
    static PrefetchDistance prefetch_distance() noexcept { return _scheduler->prefetch_distance(); }

    /**
     * Starts the runtime and suspends the starting thread until MxTasking is stopped.
     */
    static void start_and_wait() { _scheduler->start_and_wait(); }

    /**
     * Instructs all worker threads to stop their work.
     * After all worker threads are stopped, the starting
     * thread will be resumed.
     *
     * @param stop_network If set to true, the network server will also be stopped.
     */
    static void stop(const bool stop_network = true) noexcept
    {
        _scheduler->interrupt();
        if (_network_server != nullptr && stop_network)
        {
            _network_server->stop();
            _network_server_thread->join();
        }
    }

    /**
     * Creates a new task.
     *
     * @param worker_id Worker to allocate memory from.
     * @param arguments Arguments for the task.
     * @return The new task.
     */
    template <typename T, typename... Args> static T *new_task(const std::uint16_t worker_id, Args &&...arguments)
    {
        static_assert(sizeof(T) <= config::task_size() && "Task must be <= defined task size.");
        return new (_task_allocator->allocate(worker_id)) T(std::forward<Args>(arguments)...);
    }

    /**
     * Frees a given task.
     *
     * @param worker_id Worker id to return the memory to.
     * @param task Task to be freed.
     */
    template <typename T> static void delete_task(const std::uint16_t worker_id, T *task) noexcept
    {
        task->~T();
        _task_allocator->free(worker_id, static_cast<void *>(task));
    }

    /**
     * Creates a resource.
     *
     * @param size Size of the data object.
     * @param annotation Annotation for allocation and scheduling.
     * @param arguments Arguments for the data object.
     * @return The resource pointer.
     */
    template <typename T, typename... Args>
    static mx::resource::ptr new_resource(const std::size_t size, mx::resource::annotation &&annotation,
                                          Args &&...arguments) noexcept
    {
        return _resource_builder->build<T>(_worker_id, size, std::move(annotation), std::forward<Args>(arguments)...);
    }

    /**
     * Creates a resource from a given pointer.
     *
     * @param object Pointer to the existing object.
     * @param annotation Annotation for allocation and scheduling.
     * @return The resource pointer.
     */
    template <typename T>
    static mx::resource::ptr to_resource(T *object, mx::resource::annotation &&annotation) noexcept
    {
        return _resource_builder->build<T>(object, annotation);
    }

    /**
     * Deletes the given data object.
     *
     * @param resource Data object to be deleted.
     */
    template <typename T> static void delete_resource(const mx::resource::ptr resource) noexcept
    {
        _resource_builder->destroy<T>(_worker_id, resource);
    }

    /**
     * Creates a new task squad.
     *
     * @param size Size of the task squad object.
     * @param worker_id Worker to map the squad to.
     * @param arguments Arguments for the task squad object.
     * @return A resource pointer to the squad.
     */
    template <typename T, typename... Args>
    static mx::resource::ptr new_squad(const std::size_t size, const std::uint16_t worker_id,
                                       Args &&...arguments) noexcept
    {
        static_assert(std::is_base_of<TaskSquad, T>::value && "Squads needs to extend mx::tasking::TaskSquad");

        auto annotation = resource::annotation{worker_id, synchronization::isolation_level::Exclusive,
                                               synchronization::protocol::Batched};
        return _resource_builder->build<T>(_worker_id, size, std::move(annotation), std::forward<Args>(arguments)...);
    }

    /**
     * Flushes the given task squad.
     */
    static void flush_squad(resource::ptr task_squad) noexcept { task_squad.get<TaskSquad>()->flush(); }

    /**
     * Creates a simple task squad.
     *
     * @param worker_id Worker to map the squad to.
     * @return A resource pointer to the squad.
     */
    static mx::resource::ptr new_squad(const std::uint16_t worker_id) noexcept
    {
        auto annotation = resource::annotation{worker_id, synchronization::isolation_level::Exclusive,
                                               synchronization::protocol::Batched};
        return _resource_builder->build<TaskSquad>(_worker_id, sizeof(TaskSquad), std::move(annotation));
    }

    /**
     * Deletes the given data squad.
     *
     * @param resource Squad to be deleted.
     */
    template <typename T> static void delete_squad(const mx::resource::ptr resource) noexcept
    {
        _resource_builder->destroy<T>(_worker_id, resource);
    }

    /**
     * Allocates memory from the worker-local heap.
     *
     * @param numa_node_id NUMA node id where to allocate memory from.
     * @param alignment Alighment of the allocation.
     * @param size Size to allocate.
     * @return Pointer to the allocated memory.
     */
    static void *allocate(const std::uint8_t numa_node_id, const std::size_t alignment, const std::size_t size) noexcept
    {
        return _resource_allocator->allocate(_worker_id, numa_node_id, alignment, size);
    }

    /**
     * Frees a region allocated from the worker-local heap.
     *
     * @param pointer Pointer to memory.
     */
    static void free(void *pointer) noexcept { _resource_allocator->free(_worker_id, pointer); }

    /**
     * Spawns a task for every worker thread to release the free memory.
     */
    static void defragment()
    {
        const auto local_worker_id = _worker_id;
        for (auto worker_id = std::uint16_t(0U); worker_id < workers(); ++worker_id)
        {
            auto *clean_up_task =
                new_task<memory::dynamic::local::CleanUpMemoryTask>(local_worker_id, *_resource_allocator);
            clean_up_task->annotate(worker_id);
            spawn(*clean_up_task, local_worker_id);
        }
    }

    /**
     * Updates the prediction of a data object.
     *
     * @param resource Data object, whose usage should be predicted.
     * @param old_prediction Prediction so far.
     * @param new_prediction New usage prediction.
     */
    static void modify_predicted_usage(const mx::resource::ptr resource,
                                       const mx::resource::expected_access_frequency old_prediction,
                                       const mx::resource::expected_access_frequency new_prediction) noexcept
    {
        _scheduler->modify_predicted_usage(resource.worker_id(), old_prediction, new_prediction);
    }

    /**
     * ID of the NUMA region of a worker.
     * @param worker_id Worker.
     * @return ID of the NUMA region.
     */
    static std::uint8_t numa_node_id(const std::uint16_t worker_id) noexcept
    {
        return _scheduler->numa_node_id(worker_id);
    }

    /**
     * Start profiling of idle times.
     */
    static void start_idle_profiler() noexcept { _scheduler->start_idle_profiler(); }

    /**
     * Stops idle profiling.
     *
     * @return List of idle times for every channel.
     */
    [[nodiscard]] static profiling::IdleTimes stop_idle_profiler() { return _scheduler->stop_idle_profiler(); }

    /**
     * Reads the task statistics for a given counter and all channels.
     * @return List of all counters for every channel.
     */
    static std::unordered_map<profiling::TaskCounter::Counter, profiling::WorkerTaskCounter> task_counter() noexcept
    {
        if constexpr (config::is_use_task_counter())
        {
            return _scheduler->task_counter()->get();
        }
        else
        {
            return std::unordered_map<profiling::TaskCounter::Counter, profiling::WorkerTaskCounter>{};
        }
    }

    /**
     * Reads the task statistics for a given counter and all channels.
     * @param counter Counter to be read.
     * @return Aggregated value of all channels.
     */
    static profiling::WorkerTaskCounter task_counter(
        [[maybe_unused]] const profiling::TaskCounter::Counter counter) noexcept
    {
        if constexpr (config::is_use_task_counter())
        {
            return _scheduler->task_counter()->get(counter);
        }
        else
        {
            return profiling::WorkerTaskCounter{_scheduler->core_set().count_cores()};
        }
    }

    /**
     * Reads the task task_counter for a given counter on a given channel.
     * @param counter Counter to be read.
     * @param worker_id Worker.
     * @return Value of the counter of the given channel.
     */
    static std::uint64_t task_counter([[maybe_unused]] const profiling::TaskCounter::Counter counter,
                                      [[maybe_unused]] const std::uint16_t worker_id) noexcept
    {
        if constexpr (config::is_use_task_counter())
        {
            return _scheduler->task_counter()->get(counter, worker_id);
        }
        else
        {
            return 0U;
        }
    }

    static void register_task_for_trace([[maybe_unused]] const std::uint64_t task_id,
                                        [[maybe_unused]] std::string &&name)
    {
        _scheduler->task_tracer()->register_task(task_id, std::move(name));
    }

    static std::string task_name([[maybe_unused]] const std::uint64_t task_id)
    {
        auto name = _scheduler->task_tracer()->get(task_id);
        return name.value_or(std::to_string(task_id));
    }

    static void start_tracing()
    {
        if constexpr (config::is_collect_task_traces())
        {
            _scheduler->task_tracer()->start();
        }
    }

    [[nodiscard]] static profiling::TaskTraces stop_tracing()
    {
        if constexpr (config::is_collect_task_traces())
        {
            return _scheduler->task_tracer()->stop();
        }
        else
        {
            return profiling::TaskTraces{};
        }
    }

    static void listen_on_port(std::unique_ptr<io::network::MessageHandler> &&message_handler, const std::uint16_t port)
    {
        _network_server =
            std::make_unique<io::network::Server>(std::move(message_handler), port, _scheduler->count_cores());
        _network_server_thread = std::make_unique<std::thread>([] { mx::tasking::runtime::_network_server->listen(); });
    }

    static void send_message(const std::uint32_t client_id, std::string &&message)
    {
        _network_server->send(client_id, std::move(message));
    }

    static bool is_listening()
    {
        if (_network_server == nullptr)
        {
            return false;
        }

        return _network_server->is_running();
    }

    static void initialize_worker(const std::uint16_t worker_id)
    {
        _worker_id = worker_id;
        _resource_allocator->initialize_heap(worker_id, _scheduler->count_numa_nodes());
    }

    /**
     * @return The id of the executing worker. May be costly, use it carefully.
     */
    [[nodiscard]] static std::uint16_t worker_id() noexcept { return _worker_id; }

    [[nodiscard]] static std::unordered_map<std::string, std::vector<std::pair<std::uintptr_t, std::uintptr_t>>>
    memory_tags()
    {
        auto tags = _scheduler->memory_tags();

        for (auto &[name, ranges] : _task_allocator->allocated_chunks())
        {
            if (auto iterator = tags.find(name); iterator != tags.end())
            {
                std::move(ranges.begin(), ranges.end(), std::back_inserter(iterator->second));
            }
            else
            {
                tags.insert(std::make_pair(std::move(name), std::move(ranges)));
            }
        }

        return tags;
    }

private:
    inline static thread_local std::uint16_t _worker_id{std::numeric_limits<std::uint16_t>::max()};

    // Scheduler to spawn tasks.
    inline static std::unique_ptr<Scheduler> _scheduler{nullptr};

    // Allocator to allocate tasks (could be systems malloc or our Multi-level allocator).
    inline static std::unique_ptr<memory::TaskAllocatorInterface> _task_allocator{nullptr};

    // Allocator to allocate resources.
    inline static std::unique_ptr<memory::dynamic::local::Allocator> _resource_allocator{nullptr};

    // Allocator to allocate data objects.
    inline static std::unique_ptr<mx::resource::Builder> _resource_builder{nullptr};

    // Optional network server.
    inline static std::unique_ptr<io::network::Server> _network_server{nullptr};
    inline static std::unique_ptr<std::thread> _network_server_thread{nullptr};
};

/**
 * The runtime_guard initializes the runtime at initialization and starts
 * the runtime when the object is deleted. This allows MxTasking to execute
 * within a specific scope.
 */
class runtime_guard
{
public:
    runtime_guard(Libc::Env &env, const bool use_system_allocator, const util::core_set &core_set,
                  const PrefetchDistance prefetch_distance = PrefetchDistance{0U}) noexcept
    {
        runtime::init(core_set, prefetch_distance, use_system_allocator, env);
    }

    runtime_guard(Libc::Env &env, const util::core_set &core_set,
                  const PrefetchDistance prefetch_distance = PrefetchDistance{0U}) noexcept
        : runtime_guard(env, false, core_set, prefetch_distance)
    {
    }

    ~runtime_guard() noexcept { runtime::start_and_wait(); }
};
} // namespace mx::tasking
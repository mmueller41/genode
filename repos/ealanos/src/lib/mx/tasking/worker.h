#pragma once

#include "base/thread.h"
#include "channel.h"
#include "config.h"
#include "debug/helper_functions.h"
#include "profiling/statistic.h"
#include "task.h"
#include "task_stack.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mx/memory/reclamation/epoch_manager.h>
#include <mx/util/maybe_atomic.h>
#include <variant>
#include <vector>
#include <tukija/syscalls.h>
#include <base/log.h>
#include <mx/util/bound_mpmc_queue.h>
#include <mx/util/field_alloc.h>
#include <mx/util/queue.h>
#include <random>

namespace mx::tasking {
/**
 * The worker executes tasks from his own channel, until the "running" flag is false.
 */
class alignas(64) Worker
{
public:
    Worker(std::uint16_t id, std::uint16_t target_core_id, std::uint16_t target_numa_node_id,
           const util::maybe_atomic<bool> &is_running, util::Field_Allocator<config::max_cores()> &v, std::atomic<std::int32_t> &e, std::uint16_t prefetch_distance,
           memory::reclamation::LocalEpoch &local_epoch, const std::atomic<memory::reclamation::epoch_t> &global_epoch,
           profiling::Statistic &statistic) noexcept;

    ~Worker() noexcept = default;

    /**
     * Starts the worker (typically in its own thread).
     */
    void execute();

    static void *entry(void *args) { 
        Worker *worker = static_cast<Worker *>(args);
        if (worker == nullptr) {
            Genode::error("No worker found.");
            return nullptr;
        }
        worker->execute();
        return nullptr;
    }

    [[nodiscard]] std::uint16_t number_of_channels() { return _count_channels; }

    Channel *get(std::uint64_t idx);

    inline bool pick_channel(std::uint64_t offset, std::uint64_t limit, bool change_phase_to_normal = false)
	{
        //Genode::Trace::Timestamp deq_start = Genode::Trace::timestamp();
        std::uint64_t cidx = _vacant_channels.alloc_randomly( offset, limit);
        //Genode::Trace::Timestamp deq_stop = Genode::Trace::timestamp();
        //_mean_dequeue_cost += deq_stop - deq_start;
        //dequeues++;


        if (cidx > 0 && cidx < 64) {
            //Genode::log("Worker ", _id, "(", _phys_core_id, ") picked channel ", cidx);
            Channel *loot = get(cidx);

            if (change_phase_to_normal)
                loot->phase(priority::normal);
            
            //Genode::Trace::Timestamp enq_start = Genode::Trace::timestamp();
            assign(loot);
            //Genode::Trace::Timestamp enq_stop = Genode::Trace::timestamp();
            //_mean_enqueue_cost += enq_stop - enq_start;
            //enqueues++;

            return true;
        }
        return false;
    }

    inline bool steal(bool init=true)
    {

		if (!_is_running || !_may_steal) {
            return false;
        }

		if (!_vacant_channels.has_free_fields())
			return false;

        bool got_loot = false;
        /* First steal up to individual stealing limit */
        if (init) {
            while (individual_stealing_limit() > 0)
            {
				if ((got_loot |= pick_channel(_id, stealing_limit()))) continue;
				if (!got_loot) break;
            }
		} else if (_excess_queues.load(std::memory_order_relaxed) <= 0) {
			got_loot |= pick_channel(1, Tukija::Cip::cip()->channel_info.count, true);
        }

        return got_loot;
    }

    inline bool yield_signaled() { return __atomic_load_n(&(Tukija::Cip::cip()->worker_for_location(Genode::Thread::myself()->affinity()).yield_flag), __ATOMIC_RELAXED) == 1; }

    inline std::uint16_t stealing_limit() {
        return static_cast<std::uint16_t>(Tukija::Cip::cip()->channel_info.limit);
    }

    inline std::uint16_t remaining_queues() { return static_cast<std::uint16_t>(Tukija::Cip::cip()->channel_info.remainder); }

    /**
     * Assign a channel to this worker
     * This method is called by the scheduler to assign each worker an initial channel upon start. That's because initially none of the workers has any channel assigned yet. If it would try to steal one from another worker upon initialization it would not find a channel to begin with. Since this would apply for all channels the application would just stall forever, never finding a channel to steal.
    */
    void assign(Channel *channel) { _channels.push_back(channel);
        _count_channels++;
    }

    /**
     * @return Id of the logical core this worker runs on.
     */
    [[nodiscard]] std::uint16_t core_id() const noexcept { return _target_core_id; }

    /*[[nodiscard]] Channel &channel() noexcept { return _channel; }
    [[nodiscard]] const Channel &channel() const noexcept { return _channel; }*/

    [[nodiscard]] std::uint16_t numa_id() const noexcept { return _target_numa_node_id; }

    /**
     * @return Id of the physical core this worker runs on.
     */
    [[nodiscard]] std::uint16_t phys_core_id() const noexcept { return _phys_core_id; }

    /**
     * @return Id of this worker
    */
    [[nodiscard]] std::uint16_t id() const noexcept { return _id; }

    /**
     * @return the number of channels this worker currently owns.
    */
    [[nodiscard]] std::uint16_t count_channels() { return _count_channels; }

    /**
     * 
    */
    inline Channel *current_channel() { return current; }

    /**
     * Yields a number of channels except the channel given
     * @param num, the number of channels to yield
     * @param channel, the channel to keep
    */
    void yield_channels(std::uint16_t num, Channel *except);

private:

    // Id of the logical core.
    const std::uint16_t _target_core_id;

    const std::uint16_t _target_numa_node_id;

    // Distance of prefetching tasks.
    const std::uint16_t _prefetch_distance;

    std::uint16_t _phys_core_id{0};

    std::uint16_t _id{0};

    mx::synchronization::Spinlock _channel_lock{};

    // std::int32_t _channel_size{0U};

    // Stack for persisting tasks in optimistic execution. Optimistically
    // executed tasks may fail and be restored after execution.
    alignas(64) TaskStack _task_stack;

    // Channel where tasks are stored for execution.
    alignas(64) util::Queue<Channel> _channels{};

    alignas(64) Channel *current{nullptr};

    std::mt19937 _rng;

    /**
     * Profiling data structures
    */
    alignas(64) unsigned long _thefts{0};
    alignas(64) Genode::Trace::Timestamp _stealing_cost{0};
    alignas(64) Genode::Trace::Timestamp _max_cost{0};
    alignas(64) Genode::Trace::Timestamp _min_cost{0};
    alignas(64) Genode::Trace::Timestamp _mean_enqueue_cost{0};
    alignas(64) Genode::Trace::Timestamp _mean_dequeue_cost{0};
    alignas(64) unsigned long enqueues{1};
    alignas(64) unsigned long dequeues{1};

    // Local epoch of this worker.
    memory::reclamation::LocalEpoch &_local_epoch;

    // Global epoch.
    const std::atomic<memory::reclamation::epoch_t> &_global_epoch;

    // Statistics container.
    profiling::Statistic &_statistic;

    // Flag for "running" state of MxTasking.
    const util::maybe_atomic<bool> &_is_running;

    // Reference to queue of vacant channels
    util::Field_Allocator<config::max_cores()> &_vacant_channels;

    // Flag whether this worker may steal channels or not
    bool _may_steal{true};

    // Global number of excess queues
    alignas(64) std::atomic<std::int32_t> &_excess_queues;

    // Number of channels currently owned by this worker
    std::atomic<std::uint16_t> _count_channels{0};

    // Flag for "sleeping" state of this worker
    util::maybe_atomic<bool> _is_sleeping{false};

    void sleep() { _is_sleeping = true;
        Tukija::release(Tukija::Resource_type::CPU_CORE);
    }

	void yield()
	{
		_is_sleeping = true;
		Genode::log("Worker ", _id, " yielding");
        Tukija::return_to_owner(Tukija::Resource_type::CPU_CORE);
    }

    inline std::int32_t individual_stealing_limit() { std::int32_t limit = static_cast<std::int32_t>(stealing_limit()) - static_cast<std::int32_t>(_count_channels);
        return limit;
    }

    /**
     * Analyzes the given task and chooses the execution method regarding synchronization.
     * @param task Task to be executed.
     * @return Synchronization method.
     */
    static synchronization::primitive synchronization_primitive(TaskInterface *task) noexcept
    {
        return task->has_resource_annotated() ? task->annotated_resource().synchronization_primitive()
                                              : synchronization::primitive::None;
    }

    /**
     * Executes a task with a latch.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_exclusive_latched(std::uint16_t core_id, std::uint16_t channel_id, TaskInterface *task);

    /**
     * Executes a task with a reader/writer latch.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    static TaskResult execute_reader_writer_latched(std::uint16_t core_id, std::uint16_t channel_id,
                                                    TaskInterface *task);

    /**
     * Executes the task optimistically.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_optimistic(std::uint16_t core_id, std::uint16_t channel_id, TaskInterface *task);

    /**
     * Executes the task using olfit protocol.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_olfit(std::uint16_t core_id, std::uint16_t channel_id, TaskInterface *task);

    /**
     * Executes the read-only task optimistically.
     * @param core_id Id of the core.
     * @param channel_id Id of the channel.
     * @param resource Resource the task reads.
     * @param task Task to be executed.
     * @return Task to be scheduled after execution.
     */
    TaskResult execute_optimistic_read(std::uint16_t core_id, std::uint16_t channel_id,
                                       resource::ResourceInterface *resource, TaskInterface *task);

    inline void wait_for_hooter()
    {
        while (this->_is_running == false)
        {
            system::builtin::pause();
        }
    }

    inline void handle_yield()
    {
        if (yield_signaled()) {
            _may_steal = true;
            //Genode::log("Got yield signal ", _phys_core_id);
            yield_channels(_count_channels, nullptr);
            _excess_queues.fetch_sub(1);

            deregister();
            yield();
            //Genode::log("Worker on CPU ", _phys_core_id, " returned.");
            wait_for_hooter();
            _thefts = 0;
            /*
            _stealing_cost = 0;
            _max_cost = 0;
            _min_cost = 0;
            _mean_enqueue_cost = 0;
            _mean_dequeue_cost = 0;
            enqueues = 1;
            dequeues = 1;*/
            handle_resume();
        }
    }

    inline void handle_stop()
    {
		if (!_is_running) {
            _may_steal = true;
            unsigned long expect = 0;
            if (yield_signaled()) {
                handle_yield();
                return;
            }

			//Genode::log("Worker ", _id, ": attempted thefts: ", dequeues);
			//Genode::log("Worker ", _id, ": thefs=", _thefts);
            // Genode::log("Worker ", _id, ": thefts=", _thefts, " cost_total=", _stealing_cost, "avg cost_per_theft=",
            // _stealing_cost/_thefts, " min cost/theft=", _min_cost, " max cost/theft=", _max_cost, " avg deq=",
            // _mean_dequeue_cost/dequeues, " avg enq=", _mean_enqueue_cost/enqueues, " #deqs=", dequeues, " #enqs=",
            // enqueues);
            //   Genode::log("Worker ", _id, " woke up again");
            yield_channels(_count_channels, nullptr);
            deregister();
            sleep();

            
            if (yield_signaled()) {
                handle_yield();
                return;
            }

            wait_for_hooter();
			//_thefts = 0;
			//dequeues = 1;
            /*_stealing_cost = 0;
            _max_cost = 0;
            _min_cost = 0;
            _mean_enqueue_cost = 0;
            _mean_dequeue_cost = 0;
            enqueues = 1;
            dequeues = 1;*/
            handle_resume();
        }
    }
    
    inline void handle_resume()
    {
        if (!current) {
			_may_steal = true;
            if (!steal() )
            {
                handle_yield();
                handle_stop();
            }
			_excess_queues.fetch_sub(1);
            //Genode::log("Worker ", _id, " stole ", static_cast<std::uint32_t>(_count_channels), " channels.");
            current = _channels.pop_front();
            registrate();
        }
    }

    void handle_channel_occupancy()
    {
        if (_may_steal && current->has_excessive_usage_prediction()) {
            _may_steal = false;
            yield_channels(_count_channels - 1, current);
            //Genode::log("Worker ", _id, ": Got channel ", current->id(), " with excessive usage prediction.");
        }
    }

    void registrate();
    void deregister();
};
} // namespace mx::tasking
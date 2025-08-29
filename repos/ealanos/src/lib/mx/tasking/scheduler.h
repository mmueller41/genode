#pragma once
#include "channel.h"
#include "task.h"
#include "tukija/syscall-generic.h"
#include "tukija/syscalls.h"
#include "worker.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mx/memory/config.h>
#include <mx/memory/dynamic_size_allocator.h>
#include <mx/memory/reclamation/epoch_manager.h>
#include <mx/resource/resource.h>
#include <mx/tasking/profiling/profiling_task.h>
#include <mx/tasking/profiling/statistic.h>
#include <mx/util/core_set.h>
#include <mx/util/random.h>
#include <string>
#include <cmath>
#include <mx/util/field_alloc.h>

namespace mx::tasking {
/**
 * The scheduler is the central (but hidden by the runtime) data structure to spawn
 * tasks between worker threads.
 */
class Scheduler
{
public:
    Scheduler(const util::core_set &core_set, std::uint16_t prefetch_distance,
              memory::dynamic::Allocator &resource_allocator) noexcept;
    ~Scheduler() noexcept;

    /**
     * Schedules a given task.
     * @param task Task to be scheduled.
     * @param current_channel_id Channel, the request came from.
     */
    void schedule(TaskInterface &task, std::uint16_t current_channel_id) noexcept;

    /**
     * Schedules a given task.
     * @param task Task to be scheduled.
     */
    void schedule(TaskInterface &task) noexcept;

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
        while (Tukija::Cip::cip()->cores_current.count() > 1) {
			system::builtin::pause();
        }

		unsigned pcpu = Tukija::Cip::cip()->get_cpu_index();
		
        Worker *me = _worker_at_core[pcpu];

        /* We assume that the runtime is always stopped by the foreman.
         * So we should check whether this is truly the case here.
         */
        assert(me->current_channel()->id() == 0 && "Channel is not 0.");
        assert(me->id() == 0 && "Stop called by worker.");

        me->yield_channels(config::max_cores(), _channels[0]);

       // Genode::log("Got ", _vacant_channels.size(), " vacant channels.");
        // this->_profiler.stop();
    }

    void resume() noexcept { 
        //Genode::log("Allocation before resume ", allocation);

		unsigned current_cores = Tukija::Cip::cip()->cores_current.count();
		allocate_cores(_count_channels - current_cores);
		_is_running = true;
    }

    [[nodiscard]] inline Worker *my_self() noexcept {
        unsigned pcpu = Tukija::Cip::cip()->get_cpu_index();
		
        return _worker_at_core[pcpu];
    }

    [[nodiscard]] inline Channel *get_channel(std::uint64_t index) { return _channels[index]; }

    [[nodiscard]] inline std::uint16_t active_workers() const noexcept { return _active_worker_count; }

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
     * @return Number of all channels.
     */
    [[nodiscard]] std::uint16_t count_channels() const noexcept { return _count_channels; }

    /**
     * Reads the NUMA region of a given channel/worker thread.
     * @param channel_id Channel.
     * @return NUMA region of the given channel.
     */
    [[nodiscard]] std::uint8_t numa_node_id(const std::uint16_t channel_id) const noexcept
    {
        return _channel_numa_node_map[channel_id];
    }

    /**
     * Predicts usage for a given channel.
     * @param channel_id Channel.
     * @param usage Usage to predict.
     */
    void predict_usage(const std::uint16_t channel_id, const resource::hint::expected_access_frequency usage) noexcept
    {
        _channels[channel_id]->predict_usage(usage);
        /*if (usage == resource::hint::expected_access_frequency::excessive) {
            Worker *owner = _owner_of_channel[channel_id];
            
            if (owner) {
                owner->prohibit_stealing();
                //Genode::log("Worker ", owner->core_id(), " is going to have excessive load.");
                _overloaded_workers.push_back(owner);
                Nova::mword_t allocation;
                std::uint16_t channels = owner->count_channels();

                std::uint16_t cores_needed = (channels > 1) ? channels : 1;
                Nova::alloc_cores(cores_needed + _vacant_channel_count.load(), allocation);
            }
            else
            {
                Nova::mword_t allocation;
                Nova::alloc_cores(_vacant_channel_count, allocation);
            }
        }*/
    }

    /**
     * Updates the predicted usage of a channel.
     * @param channel_id Channel.
     * @param old_prediction So far predicted usage.
     * @param new_prediction New prediction.
     */
    void modify_predicted_usage(const std::uint16_t channel_id,
                                const resource::hint::expected_access_frequency old_prediction,
                                const resource::hint::expected_access_frequency new_prediction) noexcept
    {
        _channels[channel_id]->modify_predicted_usage(old_prediction, new_prediction);
    }

    /**
     * @param channel_id Channel.
     * @return True, when a least one usage was predicted to be "excessive" for the given channel.
     */
    [[nodiscard]] bool has_excessive_usage_prediction(const std::uint16_t channel_id) const noexcept
    {
        return _channels[channel_id]->has_excessive_usage_prediction();
    }

    /**
     * Resets the statistics.
     */
    void reset() noexcept;

	/**
	 * Resets the usage predictions for all channels. This is useful for cases
	 * where the resource schedule has changed (e.g. after a resume).
	 */
	void reset_usage_predictions()
	{
		for (auto channel : _channels) {
			channel->predict_usage(resource::hint::expected_access_frequency::normal);
		}
	}
	/**
     * Register a worker in the core to worker map
    */
    void register_worker(Worker *worker) { _worker_at_core[worker->phys_core_id()] = worker;
        _active_worker_count.fetch_add(1);
    }

    void deregister_worker() { _active_worker_count.fetch_sub(1); }

    /**
     * Aggregates the counter for all cores.
     * @param counter Statistic counter.
     * @return Aggregated value.
     */
    [[nodiscard]] std::uint64_t statistic([[maybe_unused]] const profiling::Statistic::Counter counter) const noexcept
    {
        if constexpr (config::task_statistics())
        {
            return this->_statistic.get(counter);
        }
        else
        {
            return 0U;
        }
    }

    /**
     * Reads the statistics for a given counter on a given channel.
     * @param counter Statistic counter.
     * @param channel_id Channel.
     * @return Value of the counter for the given channel.
     */
    [[nodiscard]] std::uint64_t statistic([[maybe_unused]] const profiling::Statistic::Counter counter,
                                          [[maybe_unused]] const std::uint16_t channel_id) const noexcept
    {
        if constexpr (config::task_statistics())
        {
            return this->_statistic.get(counter, channel_id);
        }
        else
        {
            return 0U;
        }
    }

    /**
     * Starts profiling of idle times and specifies the results file.
     * @param output_file File to write idle times after stopping MxTasking.
     */
    void profile(const std::string &output_file);

    bool operator==(const util::core_set &cores) const noexcept { return _core_set == cores; }

    bool operator!=(const util::core_set &cores) const noexcept { return _core_set != cores; }

    inline void allocate_cores(std :: uint16_t cores)
	{
		if (!cores) return;

		Tukija::Cip *cip = Tukija::Cip::cip();
		cip->cores_new.clear();
		cip->cores_reclaimed.clear();
		
		Tukija::alloc(Tukija::Resource_type::CPU_CORE, cores);
		_remainder_channel_count.store(cip->cores_new.count() + cip->cores_reclaimed.count());
    }

private:
    // Cores to run the worker threads on.
    const util::core_set _core_set;

    // Number of all channels.
    std::uint16_t _count_channels;

    alignas(64) std::atomic<bool> _loot_available{true};

    // Flag for the worker threads. If false, the worker threads will stop.
    // This is atomic for hardware that does not guarantee atomic reads/writes of booleans.
    alignas(64) util::maybe_atomic<bool> _is_running{false};

    alignas(64) std::atomic<std::uint16_t> _active_worker_count{0};

    // All initialized workers.
    alignas(64) std::array<Worker *, config::max_cores()> _worker{nullptr};
    alignas(64) std::array<Worker *, config::max_cores()> _worker_at_core{nullptr};

    alignas(64) std::array<Channel *, config::max_cores()> _channels{nullptr};

    alignas(64) mx::util::Field_Allocator<config::max_cores()> *_vacant_channels_alloc{nullptr};
    alignas(64) std::atomic<std::int32_t> _remainder_channel_count{0};

    // Map of channel id to NUMA region id.
    alignas(64) std::array<std::uint8_t, config::max_cores()> _channel_numa_node_map{0U};

    // Epoch manager for memory reclamation,
    alignas(64) memory::reclamation::EpochManager _epoch_manager;

    // Profiler for task statistics.
    profiling::Statistic _statistic;

    // Profiler for idle times.
    profiling::Profiler _profiler{};

    /**
     * Make a decision whether a task should be scheduled to the local
     * channel or a remote.
     *
     * @param is_readonly Access mode of the task.
     * @param primitive The synchronization primitive of the task annotated resource.
     * @param resource_channel_id Channel id of the task annotated resource.
     * @param current_channel_id Channel id where the spawn() operation is called.
     * @return True, if the task should be scheduled local.
     */
    [[nodiscard]] static inline bool keep_task_local(const bool is_readonly, const synchronization::primitive primitive,
                                                     const std::uint16_t resource_channel_id,
                                                     const std::uint16_t current_channel_id)
    {
        return (resource_channel_id == current_channel_id) ||
               (is_readonly && primitive != synchronization::primitive::ScheduleAll) ||
               (primitive != synchronization::primitive::None && primitive != synchronization::primitive::ScheduleAll &&
                primitive != synchronization::primitive::ScheduleWriter);
    }
};
} // namespace mx::tasking
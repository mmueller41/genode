#pragma once
#include "load.h"
#include "prefetch_distance.h"
#include "prefetch_slot.h"
#include "task.h"
#include "task_cycle_sampler.h"
#include "task_execution_time_history.h"
#include <array>
#include <cstdint>
#include <mx/memory/config.h>
#include <mx/system/cache.h>
#include <mx/system/cpu.h>
#include <utility>

namespace mx::tasking {
/**
 * The task buffer holds tasks that are ready to execute.
 * The buffer is realized as a ring buffer with a fixed size.
 * All empty slots are null pointers.
 */
template <std::size_t S> class TaskBuffer
{
public:
    class Slot
    {
    public:
        constexpr Slot() noexcept = default;
        ~Slot() noexcept = default;

        /**
         * Assigns the task for execution to this slot.
         * @param task Task that should be executed when the task buffer reaches this slot.
         */
        void task(TaskInterface *task) noexcept { _task = task; }

        [[nodiscard]] TaskInterface *task() const noexcept { return _task; }

        /**
         * Consumes the task of this slot and returns it.
         * @return Task that should be executed.
         */
        [[nodiscard]] TaskInterface *get() noexcept { return std::exchange(_task, nullptr); }

        /**
         * Executes the prefetch instructions (the task descriptor and its assigned resource).
         */
        void prefetch() noexcept { _prefetch_slot.prefetch(); }

        /**
         * Sets the given task for prefetching when the task buffer reaches this slot.
         * @param task Task that should be prefetched (both task descriptor and resource).
         */
        void prefetch(const resource::ptr resource, const PrefetchDescriptor descriptor) noexcept
        {
            _prefetch_slot.assign(resource, descriptor);
        }

        bool operator==(std::nullptr_t) const noexcept { return _task == nullptr; }
        bool operator!=(std::nullptr_t) const noexcept { return _task != nullptr; }

    private:
        TaskInterface *_task{nullptr};
        PrefetchSlot _prefetch_slot{};
    };

public:
    constexpr explicit TaskBuffer(const PrefetchDistance prefetch_distance) noexcept
        : _prefetch_distance(prefetch_distance)
    {
    }
    ~TaskBuffer() noexcept = default;

    /**
     * @return True, when the buffer is empty.
     */
    [[nodiscard]] bool empty() const noexcept { return _buffer[_head] == nullptr; }

    /**
     * @return Number of tasks in the buffer.
     */
    [[nodiscard]] std::uint16_t size() const noexcept
    {
        return _tail >= _head ? (_tail - _head) : (S - (_head - _tail));
    }

    /**
     * @return Number of maximal tasks of the buffer.
     */
    [[nodiscard]] constexpr auto max_size() const noexcept { return S; }

    /**
     * @return Number of free slots.
     */
    [[nodiscard]] std::uint16_t available_slots() const noexcept { return S - size(); }

    Slot &next() noexcept
    {
        auto &slot = this->_buffer[this->_head];
        this->_head = TaskBuffer<S>::normalize(this->_head + 1U);
        return slot;
    }

    [[nodiscard]] TaskInterface *task(const std::uint16_t index) const noexcept
    {
        return this->_buffer[TaskBuffer<S>::normalize(this->_head + index)].task();
    }

    [[nodiscard]] TaskInterface *head() const noexcept { return this->_buffer[this->_head].task(); }

    /**
     * Takes out tasks from the given queue and inserts them into the buffer.
     * @param from_queue Queue to take tasks from.
     * @param count Number of maximal tasks to take out of the queue.
     * @return Number of retrieved tasks.
     */
    template <class Q> std::uint16_t fill(Q &from_queue, std::uint16_t count) noexcept;

    [[nodiscard]] std::uint8_t refill_treshold() const noexcept
    {
        if (this->_prefetch_distance.is_enabled())
        {
            return this->_prefetch_distance.is_automatic() ? this->_task_cycles.size()
                                                           : this->_prefetch_distance.fixed_distance();
        }

        return 0U;
    }

    [[nodiscard]] bool is_prefetching_enabled() const noexcept { return this->_prefetch_distance.is_enabled(); }

    [[nodiscard]] TaskCycleSampler &sampler() noexcept { return _task_cycle_sampler; }

private:
    /// Prefetch distance.
    const PrefetchDistance _prefetch_distance;

    /// Index of the first element in the buffer.
    std::uint16_t _head{0U};

    /// Index of the last element in the buffer.
    std::uint16_t _tail{0U};

    /// Array with task-slots.
    std::array<Slot, S> _buffer{};

    /// History of last cycles for the last dispatched tasks.
    TaskExecutionTimeHistory _task_cycles;

    /// Sample for monitoring task cycles.
    TaskCycleSampler _task_cycle_sampler;

    /**
     * Normalizes the index with respect to the size.
     * @param index Index.
     * @return Normalized index.
     */
    [[nodiscard]] static std::uint16_t normalize(const std::uint16_t index) noexcept { return index & (S - 1U); }

    /**
     *  Normalizes the index backwards with respect to the given offset.
     * @param index Index.
     * @param offset Offset to index.
     * @return Normalized index.
     */
    [[nodiscard]] static std::uint16_t normalize_backward(const std::uint16_t index,
                                                          const std::uint16_t offset) noexcept
    {
        const auto diff = std::int32_t(index) - std::int32_t(offset);
        return diff + (static_cast<std::uint8_t>(index < offset) * S);
    }

    /**
     * Calculates the number of to-prefeteched cache lines from a given descriptor.
     *
     * @param descriptor Descriptor describing the prefetch.
     * @return Number of cache lines to prefetch.
     */
    [[nodiscard]] static std::uint16_t prefetched_cache_lines(const PrefetchDescriptor descriptor) noexcept
    {
        const auto descriptor_id = descriptor.id();
        const auto data = descriptor.data_without_descriptor_bits();
        switch (descriptor_id)
        {
        case PrefetchDescriptor::SizeNonTemporal:
        case PrefetchDescriptor::SizeTemporal:
        case PrefetchDescriptor::SizeWrite:
            return (PrefetchSizeView{data}.get()) / system::cache::line_size();
        case PrefetchDescriptor::CallbackAny:
            return (PrefetchCallbackView{data}.size()) / system::cache::line_size();
        case PrefetchDescriptor::MaskNonTemporal:
        case PrefetchDescriptor::MaskTemporal:
        case PrefetchDescriptor::MaskWrite:
            return PrefetchMaskView{data}.count();
        case PrefetchDescriptor::None:
            return 0U;
        }
    }
};

template <std::size_t S>
template <class Q>
std::uint16_t TaskBuffer<S>::fill(Q &from_queue, std::uint16_t count) noexcept
{
    if (count == 0U || from_queue.empty())
    {
        return 0U;
    }

    const auto size = S - count;
    TaskInterface *task;

    if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value)
    {
        std::tie(task, count) = from_queue.pop_front(count);
    }

    /// Prefetching at all.
    if (this->_prefetch_distance.is_enabled())
    {
        /// Prefetching with automatic calculated prefetch distance
        /// based on annotated cycles.
        if (this->_prefetch_distance.is_automatic())
        {
            for (auto i = 0U; i < count; ++i)
            {
                if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value == false)
                {
                    task = static_cast<TaskInterface *>(from_queue.pop_front());
                    if (task == nullptr)
                    {
                        return i;
                    }
                }

                /// Location where the task will be scheduled.
                const auto task_buffer_index = this->_tail;

                /// Schedule the task to the end of the task buffer.
                this->_buffer[task_buffer_index].task(task);

                /// Increment tail for the next task.
                this->_tail = TaskBuffer<S>::normalize(task_buffer_index + 1U);

                /// Schedule prefetch instruction <prefetch_distance> slots before.
                if (task->annotation().has_prefetch_hint())
                {
                    const auto &hint = task->annotation().prefetch_hint();

                    /// Calculate cycles needed by the prefetch (|lines| * fixed latency).
                    const auto prefetched_cache_lines = TaskBuffer::prefetched_cache_lines(hint.descriptor());
                    const auto needed_cycles_latency =
                        prefetched_cache_lines * memory::config::latency_per_prefetched_cache_line();

                    /// Go back (towards head) until latency is hidden by task executions.
                    const auto prefetch_distance = this->_task_cycles.prefetch_distance(needed_cycles_latency);

                    /// Schedule the prefetch to tail - prefetch distance.
                    this->_buffer[TaskBuffer<S>::normalize_backward(task_buffer_index, prefetch_distance)].prefetch(
                        hint.resource(), hint.descriptor());
                }

                /// Push task cycles (either monitored or annotated) to the history.
                const auto task_cycles = this->_task_cycle_sampler.cycles(task);
                this->_task_cycles.push(task_cycles);

                if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value)
                {
                    task = task->next();
                }
            }
        }

        /// Prefetching with fixed prefetch distance.
        else
        {
            auto prefetch_tail =
                TaskBuffer<S>::normalize_backward(this->_tail, this->_prefetch_distance.fixed_distance());

            for (auto i = 0U; i < count; ++i)
            {
                if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value == false)
                {
                    task = static_cast<TaskInterface *>(from_queue.pop_front());
                    if (task == nullptr)
                    {
                        return i;
                    }
                }

                /// Schedule task.
                this->_buffer[this->_tail].task(task);

                /// Increment tail.
                this->_tail = TaskBuffer<S>::normalize(this->_tail + 1U);

                /// Schedule prefetch instruction <prefetch_distance> slots before.
                if (size + i >= this->_prefetch_distance.fixed_distance() && task->annotation().has_prefetch_hint())
                {
                    const auto &hint = task->annotation().prefetch_hint();
                    this->_buffer[prefetch_tail].prefetch(hint.resource(), hint.descriptor());
                }

                /// Increment prefetch tail.
                prefetch_tail = TaskBuffer<S>::normalize(prefetch_tail + 1U);

                if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value)
                {
                    task = task->next();
                }
            }
        }
    }
    else
    {
        /// No prefetching.
        for (auto i = 0U; i < count; ++i)
        {
            if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value == false)
            {
                task = static_cast<TaskInterface *>(from_queue.pop_front());
                if (task == nullptr)
                {
                    return i;
                }
            }

            /// Schedule task.
            this->_buffer[this->_tail].task(task);

            /// Increment tail.
            this->_tail = TaskBuffer<S>::normalize(this->_tail + 1U);

            if constexpr (std::is_same<Q, mx::queue::List<TaskInterface>>::value)
            {
                task = task->next();
            }
        }
    }

    return count;
}
} // namespace mx::tasking
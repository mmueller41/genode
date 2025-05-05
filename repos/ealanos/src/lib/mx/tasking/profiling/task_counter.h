#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <mx/memory/global_heap.h>
#include <mx/tasking/config.h>
#include <mx/util/aligned_t.h>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mx::tasking::profiling {

class WorkerTaskCounter
{
public:
    WorkerTaskCounter() noexcept = default;

    explicit WorkerTaskCounter(const std::uint16_t count_workers) noexcept { _counter.resize(count_workers, 0U); }

    WorkerTaskCounter(WorkerTaskCounter &&) noexcept = default;
    WorkerTaskCounter(const WorkerTaskCounter &) noexcept = default;

    ~WorkerTaskCounter() noexcept = default;

    WorkerTaskCounter &operator=(WorkerTaskCounter &&) noexcept = default;

    std::uint64_t operator[](const std::size_t index) const noexcept { return _counter[index]; }
    std::uint64_t &operator[](const std::size_t index) noexcept { return _counter[index]; }

    [[nodiscard]] std::uint64_t sum() const noexcept { return std::accumulate(_counter.begin(), _counter.end(), 0U); }

    [[nodiscard]] std::uint16_t size() const noexcept { return _counter.size(); }

    WorkerTaskCounter operator-(const WorkerTaskCounter &other) const noexcept
    {
        auto counter = this->_counter;

        for (auto worker_id = 0U; worker_id < counter.size(); ++worker_id)
        {
            counter[worker_id] -= other[worker_id];
        }

        return WorkerTaskCounter{std::move(counter)};
    }

    WorkerTaskCounter &operator-=(const WorkerTaskCounter &other) noexcept
    {
        for (auto worker_id = 0U; worker_id < _counter.size(); ++worker_id)
        {
            _counter[worker_id] -= other[worker_id];
        }

        return *this;
    }

private:
    explicit WorkerTaskCounter(std::vector<std::uint64_t> &&counter) noexcept : _counter(std::move(counter)) {}

    std::vector<std::uint64_t> _counter;
};

/**
 * Collector for tasking statistics (scheduled tasks, executed tasks, ...).
 */
class TaskCounter
{
public:
    using counter_line_t = util::aligned_t<std::array<std::uint64_t, 7U>>;

    enum Counter : std::uint8_t
    {
        Dispatched,
        DispatchedLocally,
        DispatchedRemotely,
        Executed,
        ExecutedReader,
        ExecutedWriter,
        FilledBuffer
    };

    explicit TaskCounter(const std::uint16_t count_workers) noexcept : _count_workers(count_workers)
    {
        _counter = new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(counter_line_t) * count_workers))
            counter_line_t[count_workers];
        clear();
    }

    TaskCounter(const TaskCounter &) = delete;

    TaskCounter(TaskCounter &&other) noexcept
        : _count_workers(other._count_workers), _counter(std::exchange(other._counter, nullptr))
    {
    }

    ~TaskCounter() noexcept { delete[] this->_counter; }

    TaskCounter &operator=(const TaskCounter &) = delete;

    /**
     * Clears all collected statistics.
     */
    void clear() noexcept { std::memset(static_cast<void *>(_counter), 0, sizeof(counter_line_t) * _count_workers); }

    /**
     * Increment the template-given counter by one for the given channel.
     * @param worker_id Worker to increment the statistics for.
     */
    template <Counter C> void increment(const std::uint16_t worker_id) noexcept
    {
        ++_counter[worker_id].value()[static_cast<std::uint8_t>(C)];
    }

    /**
     * Read the given counter for a given channel.
     * @param counter Counter to read.
     * @param worker_id Worker the counter is for.
     * @return Value of the counter.
     */
    [[nodiscard]] std::uint64_t get(const Counter counter, const std::uint16_t worker_id) const noexcept
    {
        return _counter[worker_id].value()[static_cast<std::uint8_t>(counter)];
    }

    /**
     * Read and aggregate the counter for all channels.
     * @param counter Counter to read.
     * @return Value of the counter for all channels.
     */
    [[nodiscard]] WorkerTaskCounter get(const Counter counter) const noexcept
    {
        auto worker_counter = WorkerTaskCounter{_count_workers};
        for (auto i = 0U; i < _count_workers; ++i)
        {
            worker_counter[i] = get(counter, i);
        }

        return worker_counter;
    }

    /**
     * Read counter for all channels.
     *
     * @return List of channel counter for every counter.
     */
    [[nodiscard]] std::unordered_map<Counter, WorkerTaskCounter> get() const noexcept
    {
        auto counter = std::unordered_map<Counter, WorkerTaskCounter>{};
        counter.reserve(7U);

        counter.insert(std::make_pair(Counter::Dispatched, get(Counter::Dispatched)));
        counter.insert(std::make_pair(Counter::DispatchedLocally, get(Counter::DispatchedLocally)));
        counter.insert(std::make_pair(Counter::DispatchedRemotely, get(Counter::DispatchedRemotely)));
        counter.insert(std::make_pair(Counter::Executed, get(Counter::Executed)));
        counter.insert(std::make_pair(Counter::ExecutedReader, get(Counter::ExecutedReader)));
        counter.insert(std::make_pair(Counter::ExecutedWriter, get(Counter::ExecutedWriter)));
        counter.insert(std::make_pair(Counter::FilledBuffer, get(Counter::FilledBuffer)));

        return counter;
    }

private:
    // Number of channels to monitor.
    const std::uint16_t _count_workers;

    // Memory for storing the counter.
    counter_line_t *_counter{nullptr};
};

} // namespace mx::tasking::profiling
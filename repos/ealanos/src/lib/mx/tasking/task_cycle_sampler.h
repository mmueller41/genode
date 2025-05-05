#pragma once

#include "config.h"
#include "task.h"
#include <cstdint>
#include <tsl/robin_map.h>
#include <tuple>
#include <unordered_map>

namespace mx::tasking {
class TaskCycleSampler
{
private:
    class Sample
    {
    public:
        constexpr Sample() noexcept = default;

        explicit Sample(const std::uint64_t cycles) noexcept : _count(1U), _cycles(cycles), _average_cycles(cycles) {}

        Sample(const std::uint32_t count, const std::uint64_t cycles) noexcept
            : _count(count), _cycles(cycles), _average_cycles(cycles / count)
        {
        }

        ~Sample() noexcept = default;

        void add(const std::uint32_t cycles) noexcept
        {
            ++_count;
            _cycles += cycles;
            _average_cycles = _cycles / _count;
        }

        [[nodiscard]] std::uint32_t average() const noexcept { return _average_cycles; }

        [[nodiscard]] std::uint64_t count() const noexcept { return _count; }

        [[nodiscard]] std::uint64_t cycles() const noexcept { return _cycles; }

    private:
        /// Number of how many times this task was sampled.
        std::uint64_t _count{0U};

        /// Number of cycles sampled for this task.
        std::uint64_t _cycles{0U};

        /// Number of cycles in average (_count/_cycles).
        std::uint32_t _average_cycles{0U};
    };

public:
    TaskCycleSampler() { _task_cycles.reserve(16U); }

    ~TaskCycleSampler() = default;

    void add(const std::uint64_t task_id, const std::uint64_t cycles)
    {
        if (task_id != 0U)
        {
            if (auto iterator = _task_cycles.find(task_id); iterator != _task_cycles.end())
            {
                iterator.value().add(cycles);
            }
            else
            {
                _task_cycles.insert(std::make_pair(task_id, Sample{cycles}));
            }
        }
    }

    [[nodiscard]] std::uint32_t cycles(TaskInterface *task) const
    {
        if constexpr (config::is_monitor_task_cycles_for_prefetching())
        {
            const auto trace_id = task->trace_id();
            if (const auto iterator = _task_cycles.find(trace_id); iterator != _task_cycles.end())
            {
                return iterator.value().average();
            }
        }

        return task->annotation().cycles();
    }

    void dump()
    {
        for (const auto &task : _task_cycles)
        {
            std::cout << task.first << " = " << task.second.average() << "(" << task.second.cycles() << "/"
                      << task.second.count() << ")" << std::endl;
        }
    }

    [[nodiscard]] std::unordered_map<std::uint64_t, Sample> get() const noexcept
    {
        auto cycles = std::unordered_map<std::uint64_t, Sample>{};

        for (const auto &task : _task_cycles)
        {
            cycles.insert(std::make_pair(task.first, task.second));
        }

        return cycles;
    }

private:
    class Hash
    {
    public:
        std::size_t operator()(std::uint64_t key) const noexcept
        {
            key ^= key >> 33U;
            key *= std::uint64_t(0xff51afd7ed558ccd);
            key ^= key >> 33U;

            //            key *= std::uint64_t(0xc4ceb9fe1a85ec53);
            //            key ^= key >> 33U;

            return static_cast<std::size_t>(key);
        }
    };

    /// List of all tasks, (their average cycles, total number of sampled executions and total cycles during execution).
    tsl::robin_map<std::uint64_t, Sample, Hash> _task_cycles;
};
} // namespace mx::tasking
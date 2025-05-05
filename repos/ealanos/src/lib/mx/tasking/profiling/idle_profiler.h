#pragma once

#include "time.h"
#include <chrono>
#include <mx/tasking/task.h>
#include <mx/util/maybe_atomic.h>
#include <optional>
#include <utility>
#include <vector>

namespace mx::tasking::profiling {

/**
 * Task, that is scheduled with low priority and gets CPU time,
 * whenever no other task is available.
 * Every time the task gets executed, it will record the time range,
 * until the channel has new tasks for execution.
 */
class IdleProfileTask final : public TaskInterface
{
public:
    IdleProfileTask(util::maybe_atomic<bool> &is_running);
    ~IdleProfileTask() override = default;

    TaskResult execute(std::uint16_t worker_id) override;

    [[nodiscard]] std::vector<TimeRange> &idle_ranges() noexcept { return _idle_ranges; }

    [[nodiscard]] bool is_running() const noexcept { return _is_task_running; }

private:
    util::maybe_atomic<bool> &_is_profiler_running;
    util::maybe_atomic<bool> _is_task_running{false};
    std::vector<TimeRange> _idle_ranges;
};

/**
 * Schedules the idle/profiling task to every channel and
 * writes the memory to a given file.
 */
class IdleProfiler
{
public:
    IdleProfiler() noexcept = default;
    ~IdleProfiler();

    /**
     * Enable profiling and set the result file.
     * @param profiling_output_file File, where results should be written to.
     */
    void start() noexcept;

    /**
     * Normalizes all time ranges and writes them to the specified
     * file.
     */
    IdleTimes stop() noexcept;

    [[nodiscard]] bool is_running() const noexcept { return _is_running; }

private:
    util::maybe_atomic<bool> _is_running{false};

    // Time point of the runtime start.
    alignas(64) std::chrono::steady_clock::time_point _start;

    // List of all idle/profile tasks.
    std::vector<IdleProfileTask *> _tasks;
};

} // namespace mx::tasking::profiling
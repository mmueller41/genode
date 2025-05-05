#include "idle_profiler.h"
#include <mx/memory/global_heap.h>
#include <mx/tasking/runtime.h>

using namespace mx::tasking::profiling;

IdleProfileTask::IdleProfileTask(util::maybe_atomic<bool> &is_running) : _is_profiler_running(is_running)
{
    _idle_ranges.reserve(1U << 16U);
}

mx::tasking::TaskResult IdleProfileTask::execute(const std::uint16_t /*worker_id*/)
{
    this->_is_task_running = true;

    auto range = TimeRange{};

    //    while (this->_is_profiler_running && this->_channel.empty())
    //    {
    //        this->_channel.fill();
    //    }

    range.stop();

    if (range.nanoseconds() > 10U)
    {
        this->_idle_ranges.emplace_back(std::move(range));
    }

    this->_is_task_running = false;

    if (this->_is_profiler_running)
    {
        return tasking::TaskResult::make_succeed(this);
    }

    return tasking::TaskResult::make_null();
}

IdleProfiler::~IdleProfiler()
{
    for (auto *task : this->_tasks)
    {
        delete task;
    }
}

void IdleProfiler::start() noexcept
{
    if (this->_is_running)
    {
        return;
    }

    for (auto *task : this->_tasks)
    {
        delete task;
    }
    this->_tasks.clear();

    this->_start = std::chrono::steady_clock::now();
    this->_is_running = true;
}

// void IdleProfiler::start() noexcept
//{
////    auto *task = new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(IdleProfileTask)))
////        IdleProfileTask(this->_is_running);
////    task->annotate(channel.id());
////    task->annotate(mx::tasking::priority::low);
////    this->_tasks.push_back(task);
////    mx::tasking::runtime::spawn(*task);
//}

IdleTimes IdleProfiler::stop() noexcept
{
    this->_is_running = false;
    const auto end = std::chrono::steady_clock::now();
    const auto start = this->_start;

    auto idle_ranges = std::vector<std::vector<NormalizedTimeRange>>{};
    idle_ranges.reserve(mx::tasking::runtime::workers());

    for (auto *task : this->_tasks)
    {
        if (task == nullptr)
        {
            continue;
        }

        // Wait for the task to finish.
        //        while(channel_task->is_running());

        if (task->idle_ranges().empty() == false)
        {
            //            const auto &idle_range = task->idle_ranges();
            //            auto normalized_range = std::vector<NormalizedTimeRange>{};
            //            std::transform(idle_range.begin(), idle_range.end(), std::back_inserter(normalized_range),
            //                           [start](const auto &time_range) { return time_range.normalize(start); });
            //
            //            idle_ranges.emplace_back(std::move(normalized_range));
        }
        else
        {
            idle_ranges.emplace_back(std::vector<NormalizedTimeRange>{});
        }
    }

    return IdleTimes{std::move(idle_ranges), end - start};
}
#pragma once

#include "time.h"
#include <cstdlib>
#include <mx/system/cache.h>
#include <ealanos/util/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mx::tasking::profiling {
class alignas(system::cache::line_size()) WorkerTaskTracer
{
public:
    constexpr static auto inline SIZE = 1U << 16U;

    WorkerTaskTracer() = default;
    WorkerTaskTracer(WorkerTaskTracer &&) noexcept = default;
    WorkerTaskTracer(const WorkerTaskTracer &) = delete;

    WorkerTaskTracer &operator=(WorkerTaskTracer &&) noexcept = default;
    WorkerTaskTracer &operator=(const WorkerTaskTracer &) = delete;

    ~WorkerTaskTracer() = default;

    void emplace_back(const std::uint64_t task_id, TimeRange &&time_range)
    {
        _traces.back().emplace_back(std::make_pair(task_id, std::move(time_range)));

        if (_traces.back().size() >= _traces.back().capacity() - 1U) [[unlikely]]
        {
            _traces.emplace_back(std::vector<std::pair<std::uint64_t, TimeRange>>{});
            _traces.back().reserve(SIZE);
        }
    }

    [[nodiscard]] const std::vector<std::vector<std::pair<std::uint64_t, TimeRange>>> &traces() const noexcept
    {
        return _traces;
    }

    void clear()
    {
        _traces.clear();
        _traces.reserve(1U << 5U);
        _traces.emplace_back(std::vector<std::pair<std::uint64_t, TimeRange>>{});
        _traces.back().reserve(SIZE);
    }

private:
    std::vector<std::vector<std::pair<std::uint64_t, TimeRange>>> _traces;
};

class TaskTraces
{
public:
    TaskTraces(const std::chrono::nanoseconds start_timestamp,
               const std::unordered_map<std::uint64_t, std::string> &names,
               std::vector<std::vector<std::pair<std::uint64_t, NormalizedTimeRange>>> &&traces) noexcept
        : _start_timestamp(start_timestamp), _names(names), _traces(std::move(traces))
    {
    }

    TaskTraces() = default;

    TaskTraces(TaskTraces &&) noexcept = default;

    ~TaskTraces() = default;

    [[nodiscard]] const std::unordered_map<std::uint64_t, std::string> &names() const noexcept { return _names; }
    [[nodiscard]] const std::vector<std::vector<std::pair<std::uint64_t, NormalizedTimeRange>>> &traces() const noexcept
    {
        return _traces;
    }

    [[nodiscard]] nlohmann::json to_json() const;

private:
    std::chrono::nanoseconds _start_timestamp;
    std::unordered_map<std::uint64_t, std::string> _names;
    std::vector<std::vector<std::pair<std::uint64_t, NormalizedTimeRange>>> _traces;
};

class TaskTracer
{
public:
    TaskTracer(const std::uint16_t count_workers)
    {
        _worker_task_tracers.resize(count_workers);
        _task_trace_ids.reserve(1024U);
    }

    TaskTracer(TaskTracer &&) noexcept = default;

    ~TaskTracer() = default;

    void register_task(const std::uint64_t task_id, std::string &&name)
    {
        _task_trace_ids.insert_or_assign(task_id, std::move(name));
    }

    [[nodiscard]] std::optional<std::string> get(const std::uint64_t task_id) const noexcept
    {
        if (auto iterator = _task_trace_ids.find(task_id); iterator != _task_trace_ids.end())
        {
            return iterator->second;
        }

        return std::nullopt;
    }

    void emplace_back(const std::uint16_t worker_id, const std::uint64_t task_id, TimeRange &&time_range)
    {
        if (_is_enabled)
        {
            _worker_task_tracers[worker_id].emplace_back(task_id, std::move(time_range));
        }
    }

    void start()
    {
        _start = std::chrono::system_clock::now();

        for (auto &worker_tracer : _worker_task_tracers)
        {
            worker_tracer.clear();
        }

        _is_enabled = true;
    }

    [[nodiscard]] TaskTraces stop();

private:
    bool _is_enabled{false};
    std::chrono::system_clock::time_point _start;
    std::vector<WorkerTaskTracer> _worker_task_tracers;
    std::unordered_map<std::uint64_t, std::string> _task_trace_ids;
};
} // namespace mx::tasking::profiling
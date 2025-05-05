#include "task_tracer.h"

using namespace mx::tasking::profiling;

TaskTraces TaskTracer::stop()
{
    _is_enabled = false;

    auto traces = std::vector<std::vector<std::pair<std::uint64_t, NormalizedTimeRange>>>{};
    traces.reserve(this->_worker_task_tracers.size());

    const auto start = this->_start;
    for (auto &channel_tracer : this->_worker_task_tracers)
    {
        /// Get list of traces from channel tracer.
        const auto &channel_traces = channel_tracer.traces();

        /// Create a single list containing all traces from that channel.
        auto &channel_normalized_traces =
            traces.emplace_back(std::vector<std::pair<std::uint64_t, NormalizedTimeRange>>{});
        channel_normalized_traces.reserve(channel_traces.size() * WorkerTaskTracer::SIZE);

        /// Flatten the list of lists into a normalized list.
        for (const auto &trace_list : channel_traces)
        {
            std::transform(trace_list.begin(), trace_list.end(), std::back_inserter(channel_normalized_traces),
                           [start](const auto &trace) {
                               return std::make_pair(std::get<0>(trace), std::get<1>(trace).normalize(start));
                           });
        }

        /// Clear the channel tracer.
        channel_tracer.clear();
    }

    auto task_traces = TaskTraces{this->_start.time_since_epoch(), this->_task_trace_ids, std::move(traces)};
    this->_task_trace_ids.clear();
    return task_traces;
}

nlohmann::json TaskTraces::to_json() const
{
    auto task_times = std::unordered_map<std::uint64_t, std::uint64_t>{};

    auto traces = nlohmann::json{};
    traces["start"] = this->_start_timestamp.count();
    traces["tasks"] = nlohmann::json{};

    /// If not found, add the "unknown" tasks for all tasks without trace id.
    if (this->_names.find(0U) == this->_names.end())
    {
        traces["tasks"].emplace_back(nlohmann::json{{"id", 0U}, {"name", "Unknown"}});
        task_times.insert(std::make_pair(0U, 0U));
    }

    /// Add the registered tasks.
    for (const auto &[task_id, name] : this->_names)
    {
        traces["tasks"].emplace_back(nlohmann::json{{"id", task_id}, {"name", name}});
        task_times.insert(std::make_pair(task_id, 0U));
    }

    /// Calculate traces per channel.
    traces["traces"] = nlohmann::json{};
    for (const auto &worker : this->_traces)
    {
        auto channel_traces = nlohmann::json{};
        for (const auto &trace : worker)
        {
            const auto task_id = std::get<0>(trace);
            const auto start = std::get<1>(trace).start().count();
            const auto end = std::get<1>(trace).end().count();

            auto task = nlohmann::json{};
            task["tid"] = task_id;
            task["s"] = start;
            task["e"] = end;
            channel_traces.emplace_back(std::move(task));

            auto task_times_iterator = task_times.find(task_id);
            if (task_times_iterator != task_times.end()) [[likely]]
            {
                task_times_iterator->second += end - start;
            }
            else
            {
                task_times.insert(std::make_pair(task_id, end - start));
            }
        }
        traces["traces"].emplace_back(std::move(channel_traces));
    }

    /// Aggregate sums for every task type.
    const auto count_worker = this->_traces.size();
    for (auto &task : traces["tasks"])
    {
        if (task.contains("id"))
        {
            const auto task_id = task["id"].get<std::uint64_t>();
            auto task_iterator = task_times.find(task_id);
            if (task_iterator != task_times.end())
            {
                const auto time_ms = double(task_iterator->second) / 1000000.0;
                task["ms"] = time_ms;
                task["ms_per_worker"] = time_ms / count_worker;
            }
            else
            {
                task["ms"] = double(0);
                task["ms_per_worker"] = double(0);
            }
        }
    }

    /// Calculate idle times.
    auto min_ns = std::numeric_limits<std::uint64_t>::max();
    auto max_ns = std::numeric_limits<std::uint64_t>::min();
    for (const auto &channel : this->_traces)
    {
        if (channel.empty() == false)
        {
            min_ns = std::min<std::uint64_t>(min_ns, channel.front().second.start().count());
            max_ns = std::max<std::uint64_t>(max_ns, channel.back().second.end().count());
        }
    }

    const auto runtime_ns = (max_ns - min_ns) * count_worker;
    const auto runtime_ms = runtime_ns / 1000000.0;
    const auto task_time_ns = std::accumulate(task_times.begin(), task_times.end(), double(0),
                                              [](const auto sum, const auto &time) { return sum + time.second; });
    const auto idle_ms = double(runtime_ns - task_time_ns) / 1000000.0;
    traces["ms_idle"] = idle_ms;
    traces["ms_idle_per_worker"] = idle_ms / count_worker;

    /// Calculate task percent.
    traces["percent_idle"] = 100.0 / runtime_ms * (idle_ms / count_worker);
    for (auto &task : traces["tasks"])
    {
        if (task.contains("ms_per_worker"))
        {
            task["percent"] = 100.0 / runtime_ms * task["ms_per_worker"].get<float>();
        }
        else
        {
            task["percent"] = double(0);
        }
    }

    return traces;
}
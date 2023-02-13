#pragma once

#include "listener.h"
#include "request_scheduler.h"
#include <array>
#include <atomic>
#include <benchmark/chronometer.h>
#include <benchmark/cores.h>
#include <benchmark/workload.h>
#include <cstdint>
#include <db/index/blinktree/b_link_tree.h>
#include <memory>
#include <mx/util/core_set.h>
#include <string>
#include <vector>
#include <libc/component.h>
#include <mx/tasking/task.h>

#include <trace/timestamp.h>
#include <base/log.h>

namespace application::blinktree_benchmark {
/**
 * Benchmark executing the task-based BLink-Tree.
 */
class Benchmark final : public Listener
{
public:

    Benchmark(Libc::Env &env, benchmark::Cores &&, std::uint16_t iterations, std::string &&fill_workload_file,
              std::string &&mixed_workload_file, bool use_performance_counter,
              mx::synchronization::isolation_level node_isolation_level,
              mx::synchronization::protocol preferred_synchronization_method, bool print_tree_statistics,
              bool check_tree, std::string &&result_file_name, std::string &&statistic_file_name,
              std::string &&tree_file_name, bool profile);

    ~Benchmark() noexcept override = default;

    /**
     * @return Core set the benchmark should run in the current iteration.
     */
    const mx::util::core_set &core_set();

    /**
     * Callback for request tasks to notify they are out of
     * new requests.
     */
    void requests_finished() override;

    /**
     * Starts the benchmark after initialization.
     */
    void start();

private:
    std::uint64_t _start;
    std::uint64_t _end;

    // Collection of cores the benchmark should run on.
    benchmark::Cores _cores;

    // Number of iterations the benchmark should use.
    const std::uint16_t _iterations;

    // Current iteration within the actual core set.
    std::uint16_t _current_iteration = std::numeric_limits<std::uint16_t>::max();

    // Workload to get requests from.
    benchmark::Workload _workload;

    // Tree to run requests on.
    std::unique_ptr<db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>> _tree;

    // The synchronization mechanism to use for tree nodes.
    const mx::synchronization::isolation_level _node_isolation_level;

    // Preferred synchronization method.
    const mx::synchronization::protocol _preferred_synchronization_method;

    // If true, the tree statistics (height, number of nodes, ...) will be printed.
    const bool _print_tree_statistics;

    // If true, the tree will be checked for consistency after each iteration.
    const bool _check_tree;

    // Name of the file to print results to.
    const std::string _result_file_name;

    // Name of the file to print further statistics.
    const std::string _statistic_file_name;

    // Name of the file to serialize the tree to.
    const std::string _tree_file_name;

    // If true, use idle profiling.
    const bool _profile;

    // Number of open request tasks; used for tracking the benchmark.
    alignas(64) std::atomic_uint16_t _open_requests = 0;

    // List of request schedulers.
    alignas(64) std::vector<RequestSchedulerTask *> _request_scheduler;

    // Chronometer for starting/stopping time and performance counter.
    alignas(64) benchmark::Chronometer<std::uint16_t> _chronometer;

    /**
     * @return Name of the file to write profiling results to.
     */
    [[nodiscard]] std::string profile_file_name() const;

    friend class StartMeasurementTask;
};

class StartMeasurementTask : public mx::tasking::TaskInterface
{
    private:
        Benchmark &_benchmark;

    public:
        constexpr StartMeasurementTask(Benchmark& benchmark) : _benchmark(benchmark) {}
        ~StartMeasurementTask() override = default;

        mx::tasking::TaskResult execute(const std::uint16_t core_id, const std::uint16_t channel_id) override 
        {
            _benchmark._chronometer.start(static_cast<std::uint16_t>(static_cast<benchmark::phase>(_benchmark._workload)), _benchmark._current_iteration + 1, _benchmark._cores.current());
            //_benchmark._start = Genode::Trace::timestamp();
            return mx::tasking::TaskResult::make_remove();
        }
};
} // namespace application::blinktree_benchmark
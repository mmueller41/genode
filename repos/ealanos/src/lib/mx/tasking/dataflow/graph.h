#pragma once

#include "barrier_task.h"
#include "finalize_counter.h"
#include "node.h"
#include "pipeline.h"
#include "producer.h"
#include <bitset>
#include <chrono>
#include <mx/memory/global_heap.h>
#include <mx/synchronization/spinlock.h>
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace mx::tasking::dataflow {

template <typename T> class Graph;

/**
 * The SequentialProducingTask takes a graph node and calls the nodes produce()
 * method for an annotated amount or until the produce() call returns false.
 * The task will only spawned once; produced will be called in a loop.
 */
template <typename T> class SequentialProducingTask final : public TaskInterface
{
public:
    SequentialProducingTask(Graph<T> *graph, NodeInterface<T> *node) noexcept : _graph(graph), _node(node) {}
    ~SequentialProducingTask() noexcept override = default;

    TaskResult execute(std::uint16_t worker_id) override;

    [[nodiscard]] std::uint64_t trace_id() const noexcept override { return _node->trace_id(); }

private:
    Graph<T> *_graph;
    NodeInterface<T> *_node;
};

/**
 * The ParallelProducingTask takes a node that is annotated to produce data
 * in parallel. The node has to provide a set of resource annotations that
 * are used to produce data or a annotated number how often the node should
 * produce().
 * For each sequence, one ParallelProducingTask will be spawned.
 */
template <typename T> class ParallelProducingTask final : public TaskInterface
{
public:
    ParallelProducingTask(Graph<T> *graph, NodeInterface<T> *node, T &&data,
                          ParallelProducingFinalizeCounter finalize_counter) noexcept
        : _graph(graph), _node(node), _data(std::move(data)), _finalize_counter(finalize_counter)
    {
    }
    ~ParallelProducingTask() noexcept override = default;

    TaskResult execute(std::uint16_t worker_id) override;

    [[nodiscard]] std::uint64_t trace_id() const noexcept override { return _node->trace_id(); }

private:
    Graph<T> *_graph;
    NodeInterface<T> *_node;
    T _data;
    ParallelProducingFinalizeCounter _finalize_counter;
};

/**
 * Since the produced data may become very large, a single task that spawns
 * all parallel producing tasks may block a worker for a long time.
 * The SpawnParallelProducingTask will be spawned on every worker and spawn
 * parallel producing tasks for a partition of the data.
 */
template <typename T> class SpawnParallelProducingTask final : public TaskInterface
{
public:
    SpawnParallelProducingTask(Graph<T> *graph, NodeInterface<T> *node,
                               std::atomic_uint16_t *spawned_worker_counter) noexcept
        : _graph(graph), _node(node), _spawned_worker_counter(spawned_worker_counter)
    {
    }
    ~SpawnParallelProducingTask() noexcept override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        auto &generator = _node->annotation().token_generator();
        if (generator != nullptr) [[likely]]
        {
            /// Data spawned for this worker by this task.
            auto data = generator->generate(worker_id);

            if (data.empty() == false) [[likely]]
            {
                /// Counter of tasks spawned by this worker.
                /// Hopefully, they will be spawned at the same core.
                auto *finalize_counter =
                    new (std::aligned_alloc(system::cache::line_size(), sizeof(std::atomic_uint64_t)))
                        std::atomic_uint64_t(data.size());

                for (auto &&token : data)
                {
                    /// Spawn producing tasks with
                    auto *source_task = runtime::new_task<ParallelProducingTask<T>>(
                        worker_id, _graph, _node, std::move(token.data()),
                        ParallelProducingFinalizeCounter{_spawned_worker_counter, finalize_counter});

                    source_task->annotate(token.annotation());
                    runtime::spawn(*source_task, worker_id);
                }
            }

            /// It may happen, that a worker has no data. In this case,
            /// we may finalize the graph if all other tasks have
            /// already executed.
            else if (_spawned_worker_counter->fetch_sub(1U) == 1U)
            {
                _graph->finalize(worker_id, _node);
            }
        }

        return TaskResult::make_remove();
    }

private:
    Graph<T> *_graph;
    NodeInterface<T> *_node;
    std::atomic_uint16_t *_spawned_worker_counter;
};

/**
 * Since there are multiple ways to finalize a node (sequential, parallel, reduce),
 * we need different sort of finalize tasks. This is the abstract finalize task
 * that implements a function that calls the in_complete of the following node
 * and starts pipelines of the graph if needed.
 */
template <typename T> class AbstractFinalizeTask;

/**
 * The graph is a set of nodes that produce and consume data.
 * Every time, a node emits data to the graph, the graph makes sure
 * that the nodes successor will consume the data.
 *
 * Further, the nodes are arranged in pipelines to solve dependencies
 * between nodes. Everytime a pipeline finishes, the graph will start
 * depending pipelines.
 *
 * After executing the last node of the graph, all nodes and pipelines
 * will be removed.
 */
template <typename T> class Graph : public EmitterInterface<T>
{
public:
    friend class AbstractFinalizeTask<T>;

    Graph(const bool is_record_times = false) : _is_record_times(is_record_times)
    {
        _pipelines.reserve(1U << 3U);
        _node_pipelines.reserve(1U << 6U);
        _pipeline_dependencies.reserve(1U << 3U);
        _pipeline_dependencies_lock.unlock();

        if constexpr (config::is_record_graph_times())
        {
            if (_is_record_times)
            {
                _pipeline_start_times.reserve(_pipelines.capacity());
                _node_finish_times.reserve(1U << 6U);
            }
        }
    }

    ~Graph() override
    {
        std::for_each(_pipelines.begin(), _pipelines.end(), [](auto *pipeline) {
            pipeline->~Pipeline();
            std::free(pipeline);
        });
    }

    /**
     * @return All pipelines of the graph.
     */
    [[nodiscard]] const std::vector<Pipeline<T> *> &pipelines() const noexcept { return _pipelines; }

    /**
     * @return A list of node pairs (A,B) where A depends on B. A will be started when B finishes.
     */
    [[nodiscard]] const std::vector<std::pair<NodeInterface<T> *, NodeInterface<T> *>> &node_dependencies()
        const noexcept
    {
        return _node_dependencies;
    }

    /**
     * Emit data to the graph. The data will be consumed by the successor of the given node.
     *
     * @param worker_id Worker where emit() is called.
     * @param node The node emitting data.
     * @param data The emitted data.
     */
    void emit(const std::uint16_t worker_id, NodeInterface<T> *node, Token<T> &&data) override
    {
        if (_is_active) [[likely]]
        {
            node->out()->consume(worker_id, *this, std::move(data));

            if constexpr (config::is_count_graph_emits())
            {
                ++this->_emit_counter.at(node)[worker_id].value();
            }
        }
    }

    /**
     * Finalizes a given node. When a node with dependencies finalizes,
     * the graph will start depending pipelines.
     * When the last node of the graph finalizes, the graph will be freed
     * from memory.
     *
     * @param worker_id Worker where the node finalizes.
     * @param node Node that finalizes.
     */
    void finalize(std::uint16_t worker_id, NodeInterface<T> *node) override;

    void for_each_node(std::function<void(NodeInterface<T> *)> &&callback) const override
    {
        for (auto *pipeline : _pipelines)
        {
            for (auto *node : pipeline->nodes())
            {
                callback(node);
            }
        }
    }

    /**
     * Adds a single node to the graph.
     *
     * @param node Node to add.
     */
    void add(NodeInterface<T> *node)
    {
        auto *pipeline = make_pipeline();
        pipeline->emplace(node);

        _node_pipelines.template insert(std::make_pair(node, pipeline));
    }

    /**
     * Adds two nodes to the graph (if not added, yet) and
     * creates an edge between the nodes. Whenever the from node
     * emits data to the graph, to will consume this data.
     *
     * @param from Node that will produce data.
     * @param to Node that will consume the data emitted by from.
     */
    void make_edge(NodeInterface<T> *from_node, NodeInterface<T> *to_node)
    {
        if (_node_pipelines.contains(from_node) == false && _node_pipelines.contains(to_node) == false)
        {
            /// Both nodes will be in the same pipeline
            auto *pipeline = make_pipeline();
            pipeline->emplace(from_node);
            pipeline->emplace(to_node);

            _node_pipelines.template insert(std::make_pair(from_node, pipeline));
            _node_pipelines.template insert(std::make_pair(to_node, _node_pipelines.at(from_node)));
        }
        else if (_node_pipelines.contains(to_node) == false)
        {
            /// From is already part of a pipeline. Add "to" to the same.
            auto *pipeline = _node_pipelines.at(from_node);
            _node_pipelines.template insert(std::make_pair(to_node, pipeline));
            pipeline->emplace(to_node);
        }
        else if (_node_pipelines.contains(from_node) == false)
        {
            /// To is already part of a pipeline. Add "from" to the same.
            auto *pipeline = _node_pipelines.at(to_node);
            _node_pipelines.template insert(std::make_pair(from_node, pipeline));
            pipeline->emplace(from_node);
        }

        from_node->out(to_node);
        to_node->add_in(from_node);
    }

    /**
     * Creates a dependency between the node pair (A,B) where A will be
     * started only when B finishes.
     *
     * @param node Node with dependency.
     * @param node_to_wait_for Node that has to finish.
     */
    void make_dependency(NodeInterface<T> *node, NodeInterface<T> *node_to_wait_for)
    {
        _node_dependencies.template emplace_back(std::make_pair(node, node_to_wait_for));

        auto *node_pipeline = _node_pipelines.at(node);
        auto *wait_for_pipeline = _node_pipelines.at(node_to_wait_for);

        if (node_pipeline == wait_for_pipeline)
        {
            auto *new_pipeline = make_pipeline();
            change_pipeline(node_to_wait_for, node_pipeline, new_pipeline);
            _pipeline_dependencies.at(node_pipeline).template emplace_back(new_pipeline);
        }
        else
        {
            _pipeline_dependencies.at(node_pipeline).template emplace_back(wait_for_pipeline);
        }
    }

    /**
     * Starts the graph by spawning tasks that call produce() for all
     * nodes assigned to a pipeline without dependencies.
     *
     * @param worker_id Worker where the graph is started.
     */
    void start(const std::uint16_t worker_id)
    {
        if constexpr (config::is_count_graph_emits())
        {
            this->for_each_node([&emits = this->_emit_counter](auto *node) {
                emits.insert(std::make_pair(node, std::array<util::aligned_t<std::uint64_t>, config::max_cores()>{}));
                emits.at(node).fill(util::aligned_t<std::uint64_t>{0U});
            });
        }

        /// Start all tasks for preparatory work.
        for (auto *task : _preparatory_tasks)
        {
            runtime::spawn(*task, worker_id);
        }
        _preparatory_tasks.clear();

        /// Collect pipelines without dependencies and start them.
        auto pipelines_to_start = std::vector<Pipeline<T> *>{};
        for (auto &[pipeline, dependencies] : _pipeline_dependencies)
        {
            if (dependencies.empty())
            {
                pipelines_to_start.emplace_back(pipeline);
            }
        }

        for (auto *pipeline : pipelines_to_start)
        {
            _pipeline_dependencies.erase(pipeline);
            start(worker_id, pipeline);
        }
    }

    void interrupt() override { _is_active = false; }

    void add(std::vector<TaskInterface *> &&preparatory_tasks)
    {
        std::move(preparatory_tasks.begin(), preparatory_tasks.end(), std::back_inserter(_preparatory_tasks));
    }

    [[nodiscard]] std::uint64_t count_emitted(NodeInterface<T> *node) const noexcept
    {
        if constexpr (config::is_count_graph_emits())
        {
            const auto &iterator = this->_emit_counter.at(node);
            return std::accumulate(iterator.begin(), iterator.end(), 0U,
                                   [](const auto sum, const auto count) { return sum + count.value(); });
        }
        else
        {
            return 0U;
        }
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> start_time(Pipeline<T> *pipeline) const noexcept
    {
        if (auto iterator = _pipeline_start_times.find(pipeline); iterator != _pipeline_start_times.end())
        {
            return std::make_optional(iterator->second);
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::chrono::system_clock::time_point> finish_time(
        NodeInterface<T> *node) const noexcept
    {
        if (auto iterator = _node_finish_times.find(node); iterator != _node_finish_times.end())
        {
            return std::make_optional(iterator->second);
        }

        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::pair<NodeInterface<T> *, std::chrono::nanoseconds>> node_times() const
    {
        auto times = std::vector<std::pair<NodeInterface<T> *, std::chrono::nanoseconds>>{};

        for (auto *pipeline : _pipelines)
        {
            if (auto pipeline_iterator = _pipeline_start_times.find(pipeline);
                pipeline_iterator != _pipeline_start_times.end())
            {
                auto last_start = pipeline_iterator->second;

                for (auto *node : pipeline->nodes())
                {
                    if (auto node_iterator = _node_finish_times.find(node); node_iterator != _node_finish_times.end())
                    {
                        auto node_finish = node_iterator->second;
                        times.template emplace_back(std::make_pair(
                            node, std::chrono::duration_cast<std::chrono::nanoseconds>(node_finish - last_start)));
                        last_start = node_finish;
                    }
                }
            }
        }

        return times;
    }

private:
    /// All pipelines of the graph.
    std::vector<Pipeline<T> *> _pipelines;

    /// Map to assign each node a specific pipeline.
    std::unordered_map<NodeInterface<T> *, Pipeline<T> *> _node_pipelines;

    /// Dependencies between pipelines. Each pipeline might have multiple dependencies.
    std::unordered_map<Pipeline<T> *, std::vector<Pipeline<T> *>> _pipeline_dependencies;

    /// List of node pairs (A,B) where A has to wait until B finishes.
    std::vector<std::pair<NodeInterface<T> *, NodeInterface<T> *>> _node_dependencies;

    /// Tasks that should be executed before executing the graph to set up i.e. data structures.
    std::vector<TaskInterface *> _preparatory_tasks;

    /// Record execution times?
    bool _is_record_times;

    /// Start time of each pipeline.
    std::unordered_map<Pipeline<T> *, std::chrono::system_clock::time_point> _pipeline_start_times;

    /// End time of each node.
    std::unordered_map<NodeInterface<T> *, std::chrono::system_clock::time_point> _node_finish_times;

    /// Lock for pipeline dependencies. Will be locked whenever a pipeline finishes.
    alignas(64) synchronization::Spinlock _pipeline_dependencies_lock;
    std::atomic_uint32_t _finished_pipelines{0U};

    alignas(64) bool _is_active{true};

    alignas(64) std::unordered_map<NodeInterface<T> *,
                                   std::array<util::aligned_t<std::uint64_t>, config::max_cores()>> _emit_counter;

    [[nodiscard]] bool complete(std::uint16_t worker_id, NodeInterface<T> *node);

    /**
     * @return A new pipeline.
     */
    Pipeline<T> *make_pipeline()
    {
        auto *pipeline = _pipelines.template emplace_back(
            new (memory::GlobalHeap::allocate_cache_line_aligned(sizeof(Pipeline<T>))) Pipeline<T>());
        _pipeline_dependencies.insert(std::make_pair(pipeline, std::vector<Pipeline<T> *>{}));

        return pipeline;
    }

    /**
     * Moves a given node from a pipeline to another one.
     * All predecessors will be moved, too.
     *
     * @param node Node to move from one to another pipeline.
     * @param original_pipeline The original pipeline of the node.
     * @param new_pipeline The new pipeline of the node and its predecessors.
     */
    void change_pipeline(NodeInterface<T> *node, Pipeline<T> *original_pipeline, Pipeline<T> *new_pipeline)
    {
        if (_node_pipelines.at(node) == original_pipeline)
        {
            _node_pipelines.at(node) = new_pipeline;

            for (auto *node_in : node->in())
            {
                change_pipeline(node_in, original_pipeline, new_pipeline);
            }
        }
    }

    /**
     * Starts a given pipeline. This will spawn the produce tasks for the first node of the pipeline.
     *
     * @param worker_id Worker where start() is called.
     * @param pipeline Pipeline to start.
     */
    void start(const std::uint16_t worker_id, Pipeline<T> *pipeline)
    {
        auto *node = pipeline->nodes().front();

        if constexpr (config::is_record_graph_times())
        {
            if (this->_is_record_times)
            {
                this->_pipeline_start_times.insert(std::make_pair(pipeline, std::chrono::system_clock::now()));
            }
        }

        if (node->annotation().is_parallel() && node->annotation().is_producing())
        {
            const auto count_workers = runtime::workers();

            auto *spawned_worker_counter =
                new (std::aligned_alloc(system::cache::line_size(), sizeof(std::atomic_uint16_t)))
                    std::atomic_uint16_t(count_workers);

            for (auto target_worker_id = std::uint16_t(0U); target_worker_id < count_workers; ++target_worker_id)
            {
                auto *spawn_task =
                    runtime::new_task<SpawnParallelProducingTask<T>>(worker_id, this, node, spawned_worker_counter);

                spawn_task->annotate(std::uint16_t(target_worker_id));
                runtime::spawn(*spawn_task, worker_id);
            }
        }
        else if (node->annotation().is_producing()) /// Produce sequential.
        {
            auto *source_task = runtime::new_task<SequentialProducingTask<T>>(worker_id, this, node);
            runtime::spawn(*source_task, worker_id);
        }
        else
        {
            this->finalize(worker_id, node);
        }
    }
};

template <typename T> class AbstractFinalizeTask : public TaskInterface
{
public:
    constexpr AbstractFinalizeTask(Graph<T> *graph, NodeInterface<T> *node) noexcept : _graph(graph), _node(node) {}

    ~AbstractFinalizeTask() noexcept override = default;

    void complete(const std::uint16_t worker_id)
    {
        if (_graph->complete(worker_id, _node))
        {
            _graph = nullptr;
            _node = nullptr;
        }
    }

    [[nodiscard]] std::uint64_t trace_id() const noexcept override
    {
        if (_node != nullptr) [[likely]]
        {
            return _node->trace_id();
        }

        return TaskInterface::trace_id();
    }

protected:
    Graph<T> *_graph;
    NodeInterface<T> *_node;
};

/**
 * The sequentual finalize task will call "finalize" of a node once.
 */
template <typename T> class SequentialFinalizeTask final : public AbstractFinalizeTask<T>
{
public:
    constexpr SequentialFinalizeTask(Graph<T> *graph, NodeInterface<T> *node) noexcept
        : AbstractFinalizeTask<T>(graph, node)
    {
    }
    ~SequentialFinalizeTask() noexcept override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        const auto ressource = this->annotation().has_resource() ? this->annotation().resource() : nullptr;

        AbstractFinalizeTask<T>::_node->finalize(worker_id, *AbstractFinalizeTask<T>::_graph, true, ressource, nullptr);
        AbstractFinalizeTask<T>::complete(worker_id);

        return TaskResult::make_remove();
    }
};

template <typename T> class ParallelCompletionTask final : public AbstractFinalizeTask<T>
{
public:
    ParallelCompletionTask(Graph<T> *graph, NodeInterface<T> *node,
                           std::atomic_uint16_t *count_finalized_cores) noexcept
        : AbstractFinalizeTask<T>(graph, node), _count_finalized_workers(count_finalized_cores)
    {
    }

    ~ParallelCompletionTask() noexcept override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        const auto is_last = _count_finalized_workers->fetch_sub(1U) == 1U;
        if (is_last)
        {
            const auto is_completed = this->_node->annotation().completion_callback()->is_complete();
            if (is_completed)
            {
                std::free(_count_finalized_workers);
                AbstractFinalizeTask<T>::complete(worker_id);
            }
            else
            {
                _count_finalized_workers->store(this->_node->annotation().finalize_sequence().size());
                for (const auto finalize_data : this->_node->annotation().finalize_sequence())
                {
                    const auto target_worker_id = finalize_data.worker_id();
                    auto *completion_task = mx::tasking::runtime::new_task<ParallelCompletionTask<T>>(
                        worker_id, this->_graph, this->_node, _count_finalized_workers);
                    completion_task->annotate(target_worker_id);
                    mx::tasking::runtime::spawn(*completion_task, worker_id);
                }
            }
        }

        return TaskResult::make_remove();
    }

private:
    std::atomic_uint16_t *_count_finalized_workers{nullptr};
};

/**
 * The parallel finalize task will be spawned on every worker and call
 * the finalize of a node in parallel. Only the last executed finalize task
 * will call the in_complete of the follow-up node, keeping track by a atomic
 * counter.
 */
template <typename T> class ParallelFinalizeTask final : public AbstractFinalizeTask<T>
{
public:
    constexpr ParallelFinalizeTask(Graph<T> *graph, NodeInterface<T> *node,
                                   std::atomic_uint16_t *count_finalized_cores) noexcept
        : AbstractFinalizeTask<T>(graph, node), _count_finalized_workers(count_finalized_cores)
    {
    }

    ~ParallelFinalizeTask() noexcept override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        const auto is_last = _count_finalized_workers->fetch_sub(1U) == 1U;

        /// Let the node finalize it's results. Maybe, the node will emit some data to the graph.
        AbstractFinalizeTask<T>::_node->finalize(worker_id, *AbstractFinalizeTask<T>::_graph, is_last,
                                                 this->annotation().resource(), nullptr);

        if (is_last)
        {
            if (this->_node->annotation().has_completion_callback() &&
                this->_node->annotation().completion_callback()->is_complete() == false)
            {
                const auto &finalize_sequence = this->_node->annotation().finalize_sequence();
                _count_finalized_workers->store(finalize_sequence.size());
                for (const auto finalize_data : finalize_sequence)
                {
                    const auto target_worker_id = finalize_data.worker_id();
                    auto *completion_task = mx::tasking::runtime::new_task<ParallelCompletionTask<T>>(
                        worker_id, this->_graph, this->_node, _count_finalized_workers);
                    completion_task->annotate(target_worker_id);
                    mx::tasking::runtime::spawn(*completion_task, worker_id);
                }
            }
            else
            {
                std::free(_count_finalized_workers);
                AbstractFinalizeTask<T>::complete(worker_id);
            }
        }

        return TaskResult::make_remove();
    }

private:
    std::atomic_uint16_t *_count_finalized_workers{nullptr};
};

template <typename T> class ReduceFinalizeTask final : public AbstractFinalizeTask<T>
{
public:
    constexpr ReduceFinalizeTask(Graph<T> *graph, NodeInterface<T> *node, const mx::resource::ptr reduced_data) noexcept
        : AbstractFinalizeTask<T>(graph, node), _reduced_data(reduced_data)
    {
    }

    ~ReduceFinalizeTask() noexcept override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        AbstractFinalizeTask<T>::_node->finalize(worker_id, *AbstractFinalizeTask<T>::_graph, false,
                                                 this->annotation().resource(), _reduced_data);

        if (auto *next_task = dynamic_cast<ReduceFinalizeTask<T> *>(_follow_up_task))
        {
            if (next_task->_pending_preceding_counter.fetch_sub(1U) == 1U)
            {
                return TaskResult::make_succeed_and_remove(next_task);
            }
        }
        else if (auto *sequential_finalize_task = dynamic_cast<SequentialFinalizeTask<T> *>(_follow_up_task))
        {
            return TaskResult::make_succeed_and_remove(sequential_finalize_task);
        }

        return TaskResult::make_remove();
    }

    void follow_up_task(AbstractFinalizeTask<T> *task) noexcept { _follow_up_task = task; }

    [[nodiscard]] resource::ptr reduced_resource() const noexcept { return _reduced_data; }

private:
    /// The task that will reduce next.
    AbstractFinalizeTask<T> *_follow_up_task;

    /// The worker id that will be reduced.
    resource::ptr _reduced_data;

    /// Counts how many reduce tasks are pending.
    std::atomic_uint8_t _pending_preceding_counter{2U};
};

/**
 * Calls consume of a node for every object if the data sequentially.
 */
template <typename T> TaskResult SequentialProducingTask<T>::execute(const std::uint16_t worker_id)
{
    auto sequence = 0ULL;

    for (auto &data : this->_node->annotation().token_generator()->generate(worker_id))
    {
        this->_node->consume(worker_id, *_graph, std::move(data));
    }

    auto *finalize_task = mx::tasking::runtime::new_task<SequentialFinalizeTask<T>>(worker_id, _graph, _node);
    finalize_task->annotate(worker_id);
    return TaskResult::make_succeed_and_remove(finalize_task);
}

/**
 * For every part of the data, a single parallel producing task will be spawned.
 * Keeping track using an atomic counter, the last task will call finalize for the node.
 */
template <typename T> TaskResult ParallelProducingTask<T>::execute(const std::uint16_t worker_id)
{
    _node->consume(worker_id, *_graph, Token<T>{std::move(this->_data), this->annotation()});
    if (_finalize_counter.tick())
    {
        _graph->finalize(worker_id, _node);
    }

    return TaskResult::make_remove();
}

class FinalizeReduceCalculator
{
public:
    [[nodiscard]] static std::pair<std::vector<std::vector<std::pair<resource::ptr, resource::ptr>>>, resource::ptr>
    pairs(const std::vector<resource::ptr> &data)
    {
        auto pairs = std::vector<std::vector<std::pair<resource::ptr, resource::ptr>>>{};

        auto reduced_data = data;
        while (reduced_data.size() > 1U)
        {
            reduced_data = FinalizeReduceCalculator::reduce(pairs, std::move(reduced_data));
        }

        return std::make_pair(std::move(pairs), reduced_data.front());
    }

private:
    [[nodiscard]] static std::vector<mx::resource::ptr> reduce(
        std::vector<std::vector<std::pair<resource::ptr, resource::ptr>>> &reduce_passes,
        std::vector<resource::ptr> &&to_reduce)
    {
        /// Pairs to reduce.
        auto pairs = std::vector<std::pair<mx::resource::ptr, mx::resource::ptr>>{};
        pairs.reserve(to_reduce.size() / 2U);

        /// Data that will be reduced in the next step.
        auto to_reduce_next = std::vector<resource::ptr>{};
        to_reduce_next.reserve((to_reduce.size() / 2U) + 1U);

        const auto is_odd = to_reduce.size() % 2U != 0U;
        for (auto i = 0U; i < to_reduce.size() - static_cast<std::uint16_t>(is_odd); i += 2U)
        {
            pairs.emplace_back(std::make_pair(to_reduce[i], to_reduce[i + 1U]));
            to_reduce_next.emplace_back(to_reduce[i]);
        }

        if (is_odd)
        {
            to_reduce_next.emplace_back(to_reduce.back());
        }

        if (pairs.empty() == false)
        {
            reduce_passes.emplace_back(std::move(pairs));
        }

        return to_reduce_next;
    }
};

template <typename T> void Graph<T>::finalize(const std::uint16_t worker_id, NodeInterface<T> *node)
{
    const auto finalization_type = node->annotation().finalization_type();
    if (finalization_type == annotation<T>::FinalizationType::parallel)
    {
        auto *finalized_worker_counter =
            new (std::aligned_alloc(system::cache::line_size(), sizeof(std::atomic_uint16_t)))
                std::atomic_uint16_t(node->annotation().finalize_sequence().size());
        for (auto data : node->annotation().finalize_sequence())
        {
            data.reset(resource::information{data.worker_id(), synchronization::primitive::ScheduleAll});
            auto *finalize_task =
                runtime::new_task<ParallelFinalizeTask<T>>(worker_id, this, node, finalized_worker_counter);
            finalize_task->annotate(data);
            runtime::spawn(*finalize_task, worker_id);
        }
    }
    else if (finalization_type == annotation<T>::FinalizationType::reduce)
    {
        /// List of list of resource pairs to reduce.
        auto reduce_resource_pairs = FinalizeReduceCalculator::pairs(node->annotation().finalize_sequence());
        auto &pair_lists = std::get<0>(reduce_resource_pairs);

        /// The resource with the final finalization task (that will be a sequential finalization task).
        const auto last_resource = std::get<1>(reduce_resource_pairs);

        /// Create the last and final finalization task.
        auto *last_finalization_task = runtime::new_task<SequentialFinalizeTask<T>>(worker_id, this, node);
        last_finalization_task->annotate(last_resource);
        if (pair_lists.empty())
        {
            runtime::spawn(*last_finalization_task, worker_id);
            return;
        }

        /// Reduce tasks, one map (main worker id -> finalization task) per "reduce stage".
        auto tasks = std::vector<std::unordered_map<resource::ptr, ReduceFinalizeTask<T> *>>{};
        tasks.reserve(pair_lists.size());

        /// Create all reduce tasks.
        for (auto &pair_list : pair_lists)
        {
            auto task_map = std::unordered_map<resource::ptr, ReduceFinalizeTask<T> *>{};
            task_map.reserve(pair_list.size());

            for (auto &pair : pair_list)
            {
                const auto main_resource = std::get<0>(pair);
                const auto reduced_resource = std::get<1>(pair);
                auto *reduce_task = runtime::new_task<ReduceFinalizeTask<T>>(worker_id, this, node, reduced_resource);
                reduce_task->annotate(main_resource);
                task_map.insert(std::make_pair(main_resource, reduce_task));
            }

            tasks.template emplace_back(std::move(task_map));
        }

        /// Setup the last reduce step.
        if (auto iterator = tasks.back().find(last_resource); iterator != tasks.back().end())
        {
            iterator->second->follow_up_task(last_finalization_task);
        }

        /// Setup all follow up tasks.
        for (auto stage = tasks.size() - 1U; stage > 0U; --stage)
        {
            for (auto [main_resource, task] : tasks[stage])
            {
                /// The task for the main worker will be in the last stage.
                if (auto main_resource_task = tasks[stage - 1U].find(main_resource);
                    main_resource_task != tasks[stage - 1U].end())
                {
                    main_resource_task->second->follow_up_task(task);
                }

                /// The task for the reduced worker may be in any stage before.
                const auto reduced_resource = task->reduced_resource();
                for (auto reduced_stage = stage - 1U; reduced_stage >= 0U; --reduced_stage)
                {
                    if (auto reduced_resource_task = tasks[reduced_stage].find(reduced_resource);
                        reduced_resource_task != tasks[reduced_stage].end())
                    {
                        reduced_resource_task->second->follow_up_task(task);
                        break;
                    }
                }
            }
        }

        /// Spawn tasks of the first stage.
        for (auto [_, finalization_task] : tasks[0])
        {
            runtime::spawn(*finalization_task, worker_id);
        }
    }
    else if (finalization_type == annotation<T>::FinalizationType::none)
    {
        node->finalize(worker_id, *this, true, nullptr, nullptr);
        this->complete(worker_id, node);
    }
    else
    {
        auto *finalize_task = runtime::new_task<SequentialFinalizeTask<T>>(worker_id, this, node);
        finalize_task->annotate(mx::tasking::annotation::local);
        runtime::spawn(*finalize_task, worker_id);
    }
}

template <typename T> bool Graph<T>::complete(std::uint16_t worker_id, NodeInterface<T> *node)
{
    if constexpr (config::is_record_graph_times())
    {
        if (this->_is_record_times)
        {
            this->_node_finish_times.insert(std::make_pair(node, std::chrono::system_clock::now()));
        }
    }

    /// Tell the next node, that this node has completed.
    Pipeline<T> *next_node_pipeline{nullptr};
    if (node->out() != nullptr)
    {
        node->out()->in_completed(worker_id, *this, *node);
        next_node_pipeline = this->_node_pipelines.at(node->out());
    }

    /// Maybe, the node was the last one in it's pipeline.
    /// If so, we may trigger one or multiple pipelines that depended on the nodes pipeline.
    auto *node_pipeline = this->_node_pipelines.at(node);
    auto pipelines_to_start = std::vector<Pipeline<T> *>{};
    if (node_pipeline != next_node_pipeline || node->annotation().is_finalizes_pipeline())
    {
        this->_pipeline_dependencies_lock.lock();

        /// Remove the nodes pipeline from the dependency list of all other pipelines.
        /// If, after removing the pipeline from dependencies, there are pipelines without
        /// dependency, we can start that pipelines.
        for (auto &[pipeline, dependencies] : this->_pipeline_dependencies)
        {
            auto dependency_iterator = std::find(dependencies.begin(), dependencies.end(), node_pipeline);
            if (dependency_iterator != dependencies.end())
            {
                dependencies.erase(dependency_iterator);
            }

            if (dependencies.empty())
            {
                pipelines_to_start.emplace_back(pipeline);
            }
        }

        /// Start all pipelines without dependency.
        for (auto *pipeline : pipelines_to_start)
        {
            this->_pipeline_dependencies.erase(pipeline);
            this->start(worker_id, pipeline);
        }

        this->_pipeline_dependencies_lock.unlock();
    }

    /// If the node was has no successor and we did not start any pipeline, we finished.
    if (node->out() == nullptr)
    {
        if (this->_finished_pipelines.fetch_add(1U) == (this->_pipelines.size() - 1U))
        {
            delete this;
            return true;
        }
    }

    return false;
}
} // namespace mx::tasking::dataflow
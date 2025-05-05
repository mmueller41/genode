#pragma once

#include "barrier_task.h"
#include "node.h"
#include "producer.h"
#include "token.h"
#include <array>
#include <cstdint>
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>
#include <mx/util/aligned_t.h>
#include <optional>
#include <string>

namespace mx::tasking::dataflow {
/**
 * Task that consumes and produces data in context of nodes.
 */
template <typename T> class DataTaskInterface
{
public:
    using value_type = T;

    constexpr DataTaskInterface() noexcept = default;
    virtual ~DataTaskInterface() noexcept = default;

    /**
     * Consumes the given data.
     * New data may be emitted to the given node.
     *
     * @param worker_id Local worker id where the task is executed.
     * @param node Node that executes that task.
     * @param emitter Emitter that can emit new data.
     * @param data Data that is consumed by that task.
     */
    virtual void execute(std::uint16_t worker_id, NodeInterface<T> *node, EmitterInterface<T> &emitter,
                         Token<T> &&data) = 0;
};

enum input_cardinality
{
    single,
    multiple
};

template <class DataTask> class NodeTask;
template <typename DataTask> class TaskNode : public NodeInterface<typename DataTask::value_type>
{
    friend NodeTask<DataTask>;

public:
    using value_type = typename DataTask::value_type;

    TaskNode() noexcept = default;

    ~TaskNode() noexcept override = default;

    void add_in(NodeInterface<value_type> *in_node) noexcept override
    {
        _count_nodes_in.fetch_add(1);
        NodeInterface<value_type>::add_in(in_node);
    }

    /**
     * Consumes the data by spawning a task of type TASK_TYPE.
     *
     * @param worker_id Worker where consume is called.
     * @param graph Graph where the node is located in.
     * @param token Data that is consumed.
     */
    void consume(std::uint16_t worker_id, EmitterInterface<value_type> &graph, Token<value_type> &&token) override;

    /**
     * Called whenever the succeeding node was finalized.
     *
     * @param worker_id Core where consume is called.
     * @param graph Graph where the node is located in.
     */
    void in_completed(const std::uint16_t worker_id, EmitterInterface<value_type> &graph,
                      NodeInterface<value_type> & /*in_node*/) override
    {
        if (_count_nodes_in.fetch_sub(1) == 1)
        {
            const auto count_workers = mx::tasking::runtime::workers();
            _count_pending_workers = count_workers - 1;
            for (auto target_worker_id = std::uint16_t(0U); target_worker_id < count_workers; ++target_worker_id)
            {
                auto *barrier_task = mx::tasking::runtime::new_task<FinalizationBarrierTask<value_type>>(
                    worker_id, _count_pending_workers, graph, this);
                barrier_task->annotate(target_worker_id);
                mx::tasking::runtime::spawn(*barrier_task, worker_id);
            }
        }
    }

    [[nodiscard]] std::string to_string() const noexcept override
    {
        return std::string{"Task Skeleton ["} + typeid(DataTask).name() + "]";
    }

private:
    std::atomic_int16_t _count_nodes_in{0U};
    std::atomic_int16_t _count_pending_workers{0U};
};

/**
 * The NodeTask executes ("wraps") the DataTask of the given node and executed the node logic.
 * @tparam DataTask
 */
template <class DataTask> class NodeTask final : public TaskInterface
{
public:
    NodeTask(TaskNode<DataTask> *owning_node, EmitterInterface<typename DataTask::value_type> &graph,
             Token<typename DataTask::value_type> &&token) noexcept
        : _owning_node(owning_node), _graph(graph), _token_data(std::move(token.data()))
    {
    }
    ~NodeTask() noexcept override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        DataTask{}.execute(worker_id, _owning_node, _graph,
                           Token<typename DataTask::value_type>{std::move(_token_data), annotation()});

        return TaskResult::make_remove();
    }

    [[nodiscard]] std::uint64_t trace_id() const noexcept override { return _owning_node->trace_id(); }

private:
    TaskNode<DataTask> *_owning_node;
    EmitterInterface<typename DataTask::value_type> &_graph;

    /// Data that was consumed by the node.
    typename DataTask::value_type _token_data;
};

template <typename DataTask>
void TaskNode<DataTask>::consume(const std::uint16_t worker_id, EmitterInterface<typename DataTask::value_type> &graph,
                                 Token<typename DataTask::value_type> &&token)
{
    //    _task_counter.add(worker_id, 1);
    const auto annotation = token.annotation();
    auto *node_task = runtime::new_task<NodeTask<DataTask>>(worker_id, this, graph, std::move(token));
    node_task->annotate(annotation);

    runtime::spawn(*node_task, worker_id);
}
} // namespace mx::tasking::dataflow
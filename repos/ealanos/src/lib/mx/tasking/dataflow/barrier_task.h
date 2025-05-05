#pragma once

#include "node.h"
#include "producer.h"
#include <atomic>
#include <cstdint>
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>

namespace mx::tasking::dataflow {
/**
 * The finalization barrier is spawned on every channel that executed
 * at least one task of the TaskNode.
 * After the last finalization barrier was hit, the graph will finalize the node.
 */
template <class T> class FinalizationBarrierTask final : public TaskInterface
{
public:
    FinalizationBarrierTask(std::atomic_int16_t &counter, EmitterInterface<T> &graph, NodeInterface<T> *node) noexcept
        : _count_pending_workers(counter), _graph(graph), _node(node)
    {
    }

    ~FinalizationBarrierTask() override = default;

    TaskResult execute(const std::uint16_t worker_id) override
    {
        const auto pending_workers = _count_pending_workers.fetch_sub(1);
        if (pending_workers == 0)
        {
            _graph.finalize(worker_id, _node);
        }

        return TaskResult::make_remove();
    }

    [[nodiscard]] std::uint64_t trace_id() const noexcept override { return _node->trace_id(); }

private:
    std::atomic_int16_t &_count_pending_workers;
    EmitterInterface<T> &_graph;
    NodeInterface<T> *_node;
};
} // namespace mx::tasking::dataflow
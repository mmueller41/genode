#pragma once

#include "node.h"
#include <atomic>
#include <vector>

namespace mx::tasking::dataflow {
template <typename T> class alignas(64) Pipeline
{
public:
    Pipeline() { _nodes.reserve(1U << 4U); }

    ~Pipeline()
    {
        std::for_each(_nodes.begin(), _nodes.end(), [](auto node) { delete node; });
    }

    void emplace(NodeInterface<T> *node) { _nodes.template emplace_back(node); }
    [[nodiscard]] const std::vector<NodeInterface<T> *> &nodes() const noexcept { return _nodes; }

    [[nodiscard]] std::atomic_uint16_t &finalization_barrier_counter() noexcept
    {
        return _finalization_barrier_counter;
    }

private:
    std::vector<NodeInterface<T> *> _nodes;
    alignas(64) std::atomic_uint16_t _finalization_barrier_counter;
};
} // namespace mx::tasking::dataflow
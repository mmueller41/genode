#pragma once

#include "token.h"
#include <cstdint>

namespace mx::tasking::dataflow {
template <typename T> class NodeInterface;
template <typename T> class EmitterInterface
{
public:
    constexpr EmitterInterface() noexcept = default;
    virtual ~EmitterInterface() noexcept = default;

    virtual void emit(std::uint16_t worker_id, NodeInterface<T> *node, Token<T> &&data) = 0;
    virtual void finalize(std::uint16_t worker_id, NodeInterface<T> *node) = 0;
    virtual void interrupt() = 0;
    virtual void for_each_node(std::function<void(NodeInterface<T> *)> &&callback) const = 0;
};
} // namespace mx::tasking::dataflow
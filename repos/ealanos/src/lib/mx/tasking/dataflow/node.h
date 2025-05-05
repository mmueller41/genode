#pragma once

#include "annotation.h"
#include "producer.h"
#include "token.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace mx::tasking::dataflow {
template <typename T> class NodeInterface
{
public:
    NodeInterface() = default;
    virtual ~NodeInterface() = default;

    /**
     * Updates the successor.
     *
     * @param out New successor.
     */
    virtual void out(NodeInterface *out) noexcept { _out = out; }

    /**
     * @return The successor of this node.
     */
    [[nodiscard]] NodeInterface *out() const noexcept { return _out; }

    /**
     * Inserts the given node as a predecessor of this node.
     *
     * @param incomeing Node to insert in the predecessor list.
     */
    virtual void add_in(NodeInterface *incomeing) noexcept { _in.emplace_back(incomeing); }

    /**
     * @return A list of predecessors of this node.
     */
    [[nodiscard]] const std::vector<NodeInterface *> &in() const noexcept { return _in; }

    /**
     * Updates the annotation of this node.
     *
     * @param annotation New annotation.
     */
    void annotate(dataflow::annotation<T> &&annotation) noexcept { _annotation = std::move(annotation); }

    /**
     * @return The annotation of the node.
     */
    [[nodiscard]] const dataflow::annotation<T> &annotation() const noexcept { return _annotation; }

    /**
     * @return The annotation of the node.
     */
    [[nodiscard]] dataflow::annotation<T> &annotation() noexcept { return _annotation; }

    /**
     * Consumes data and may emit data to the graph. This function is called
     * by the graph when a predecessor emits data to the graph.
     *
     * @param worker_id Worker, where the data is consumed.
     * @param emitter Emitter that takes data when the node wants to emit data.
     * @param data Data that is consumed.
     */
    virtual void consume(std::uint16_t worker_id, EmitterInterface<T> &emitter, Token<T> &&data) = 0;

    /**
     * Callback that is called by the graph when one of the
     * incoming nodes completes its execution.
     *
     * @param worker_id Worker, where the incoming node completed.
     * @param emitter Emitter that takes data when the node wants to emit data.
     * @param in_node Node that completed.
     */
    virtual void in_completed(std::uint16_t worker_id, EmitterInterface<T> &emitter, NodeInterface<T> &in_node) = 0;

    /**
     * Callback that is called by the graph, when a nodes completes.
     *
     * @param worker_id Worker, where the node closes.
     * @param emitter Emitter that takes data when the node wants to emit data.
     * @param is_last True, if this is the last finalization call (may be interesting for parallel finalization).
     * @param data Data that is finalized, may be nullptr.
     * @param reduced_data Data that is reduced, may be nullptr.
     */
    virtual void finalize(const std::uint16_t /*worker_id*/, EmitterInterface<T> & /*emitter*/, const bool /*is_last*/,
                          const mx::resource::ptr /*data*/, const mx::resource::ptr /*reduced_data*/)
    {
    }

    [[nodiscard]] virtual std::string to_string() const noexcept = 0;

    [[nodiscard]] virtual std::uint64_t trace_id() const noexcept { return 0U; }

private:
    /// Node where data is emitted to.
    NodeInterface<T> *_out{nullptr};
    /// Nodes from where data is consumed.
    std::vector<NodeInterface<T> *> _in;

    /// Annotation.
    dataflow::annotation<T> _annotation;
};

template <typename T> class ProducingNodeInterface : public NodeInterface<T>
{
public:
    ProducingNodeInterface<T>() = default;
    ~ProducingNodeInterface<T>() override = default;

    void in_completed(const std::uint16_t /*worker_id*/, EmitterInterface<T> & /*emitter*/,
                      NodeInterface<T> & /*in_node*/) override
    {
    }
};

template <typename T> class EmptyNode final : public NodeInterface<T>
{
public:
    EmptyNode<T>() = default;
    ~EmptyNode<T>() override = default;

    void consume(const std::uint16_t /*worker_id*/, EmitterInterface<T> & /*emitter*/, Token<T> && /*data*/) override {}

    void in_completed(const std::uint16_t worker_id, EmitterInterface<T> &emitter,
                      NodeInterface<T> & /*in_node*/) override
    {
        emitter.finalize(worker_id, this);
    }

    [[nodiscard]] std::string to_string() const noexcept override { return "Empty Node"; }
};
} // namespace mx::tasking::dataflow
#pragma once
#include "token.h"
#include "token_generator.h"
#include <cstdint>
#include <mx/tasking/annotation.h>
#include <variant>
#include <vector>

namespace mx::tasking::dataflow {
template <typename T> class annotation
{
public:
    using value_type = T;

    enum FinalizationType : std::uint8_t
    {
        sequential,
        parallel,
        reduce,
        none
    };

    class CompletionCallbackInterface
    {
    public:
        constexpr CompletionCallbackInterface() noexcept = default;
        constexpr virtual ~CompletionCallbackInterface() noexcept = default;

        [[nodiscard]] virtual bool is_complete() noexcept = 0;
    };

    annotation() noexcept = default;
    annotation(annotation &&) noexcept = default;
    ~annotation() noexcept = default;

    annotation &operator=(annotation &&) noexcept = default;

    void is_parallel(const bool is_parallel) noexcept { _is_parallel = is_parallel; }

    void produces(std::unique_ptr<TokenGenerator<T>> &&generator) noexcept { _token_generator = std::move(generator); }

    void resource_boundness(const enum tasking::annotation::resource_boundness resource_boundness) noexcept
    {
        _resource_boundness = resource_boundness;
    }

    void finalization_type(const FinalizationType type) noexcept { _finalization_type = type; }
    void finalizes(std::vector<mx::resource::ptr> &&data) { _finalized_data = std::move(data); }

    void is_finalizes_pipeline(const bool is_finalizes_pipeline) noexcept
    {
        _is_finalizes_pipeline = is_finalizes_pipeline;
    }

    void completion_callback(std::unique_ptr<CompletionCallbackInterface> &&callback) noexcept
    {
        _complection_callback = std::move(callback);
    }

    [[nodiscard]] bool is_parallel() const noexcept { return _is_parallel; }
    [[nodiscard]] std::unique_ptr<TokenGenerator<T>> &token_generator() noexcept { return _token_generator; }
    [[nodiscard]] enum tasking::annotation::resource_boundness resource_boundness() const noexcept
    {
        return _resource_boundness;
    }

    [[nodiscard]] bool is_producing() const noexcept { return _token_generator != nullptr; }

    [[nodiscard]] FinalizationType finalization_type() const noexcept { return _finalization_type; }
    [[nodiscard]] const std::vector<mx::resource::ptr> &finalize_sequence() const noexcept { return _finalized_data; }
    [[nodiscard]] bool is_finalizes_pipeline() const noexcept { return _is_finalizes_pipeline; }

    [[nodiscard]] const std::unique_ptr<CompletionCallbackInterface> &completion_callback() const noexcept
    {
        return _complection_callback;
    }
    [[nodiscard]] bool has_completion_callback() const noexcept { return _complection_callback != nullptr; }

private:
    bool _is_parallel{false};
    std::unique_ptr<TokenGenerator<T>> _token_generator;

    enum tasking::annotation::resource_boundness _resource_boundness{tasking::annotation::resource_boundness::mixed};

    FinalizationType _finalization_type{FinalizationType::sequential};
    std::vector<mx::resource::ptr> _finalized_data;

    /// Pipelines are finalized, when the last node is finished.
    /// However, a node may finalize the pipeline premature.
    bool _is_finalizes_pipeline{false};

    /// Callback that evalutes if a node is "completed".
    /// Some nodes may spawn further tasks during finalization.
    /// They will complete only after executing those tasks.
    std::unique_ptr<CompletionCallbackInterface> _complection_callback{nullptr};
};
} // namespace mx::tasking::dataflow
#pragma once

#include "token.h"
#include <cstdint>
#include <mx/tasking/prefetch_descriptor.h>
#include <vector>

namespace mx::tasking::dataflow {
template <typename T> class TokenGenerator
{
public:
    constexpr TokenGenerator() noexcept = default;
    virtual ~TokenGenerator() noexcept = default;

    [[nodiscard]] virtual std::vector<Token<T>> generate(std::uint16_t worker_id) = 0;
    [[nodiscard]] virtual std::uint64_t count() = 0;
};
} // namespace mx::tasking::dataflow
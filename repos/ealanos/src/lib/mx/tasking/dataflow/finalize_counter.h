#pragma once
#include <atomic>
#include <cstdint>

namespace mx::tasking::dataflow {
class ParallelProducingFinalizeCounter
{
public:
    ParallelProducingFinalizeCounter(std::atomic_uint16_t *worker_counter, std::atomic_uint64_t *task_counter) noexcept
        : _task_counter(task_counter), _worker_counter(worker_counter)
    {
    }

    ParallelProducingFinalizeCounter(const ParallelProducingFinalizeCounter &) noexcept = default;
    ParallelProducingFinalizeCounter(ParallelProducingFinalizeCounter &&) noexcept = default;

    ~ParallelProducingFinalizeCounter() noexcept = default;

    [[nodiscard]] bool tick() noexcept
    {
        if (_task_counter->fetch_sub(1U) == 1U)
        {
            std::free(_task_counter);

            if (_worker_counter->fetch_sub(1U) == 1U)
            {
                std::free(_worker_counter);
                return true;
            }
        }

        return false;
    }

private:
    std::atomic_uint64_t *_task_counter;

    std::atomic_uint16_t *_worker_counter;
};
} // namespace mx::tasking::dataflow
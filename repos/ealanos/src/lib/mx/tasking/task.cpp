#include "task.h"
#include "runtime.h"
#include <mx/system/cpu.h>

using namespace mx::tasking;

TaskResult TaskResult::make_stop(const std::uint16_t worker_id, const bool stop_network) noexcept
{
    auto *stop_task = runtime::new_task<StopTaskingTask>(worker_id, stop_network);
    stop_task->annotate(std::uint16_t{0U});
    return TaskResult::make_succeed_and_remove(stop_task);
}

TaskResult TaskLine::execute(const std::uint16_t worker_id)
{
    auto result = this->_next_task->execute(worker_id);
    if (result.is_remove())
    {
        mx::tasking::runtime::delete_task(worker_id, this->_next_task);
    }

    if (result.has_successor())
    {
        this->_next_task = static_cast<TaskInterface *>(result);
        this->annotate(_next_task);
        return TaskResult::make_succeed(this);
    }

    if (this->_waiting_tasks.empty() == false)
    {
        this->_next_task = this->_waiting_tasks.pop_front();
        this->annotate(_next_task);
        return TaskResult::make_succeed(this);
    }

    return TaskResult::make_remove();
}

TaskResult StopTaskingTask::execute(const std::uint16_t /*worker_id*/)
{
    runtime::stop(this->_stop_network);
    return TaskResult::make_remove();
}
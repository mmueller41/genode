#include "task_squad.h"
#include "runtime.h"

using namespace mx::tasking;

void TaskSquad::flush() noexcept
{
    auto [first, last] = this->_remote_queue.pop();
    if (first != nullptr)
    {
        if (last != nullptr)
        {
            this->_local_queue.push_back(first, last);
        }
        else
        {
            this->_local_queue.push_back(first);
        }
    }
}

TaskResult TaskSquadSpawnTask::execute(std::uint16_t worker_id)
{
    this->_task_squad.flush();

    /// Get all tasks.
    auto [first, last] = this->_task_squad._local_queue.pop();
    if (first != nullptr)
    {
        if (last != nullptr)
        {
            runtime::spawn(*first, *last, worker_id);
        }
        else
        {
            first->annotate(annotation::execution_destination::local);
            runtime::spawn(*first, worker_id);
        }
    }

    return TaskResult::make_remove();
}
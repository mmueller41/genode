#pragma once

#include "task.h"
#include <mx/queue/list.h>
#include <mx/queue/mpsc.h>

namespace mx::tasking {
class TaskSquadSpawnTask;

class TaskSquad
{
    friend TaskSquadSpawnTask;

public:
    TaskSquad() = default;
    virtual ~TaskSquad() = default;

    void push_back_local(TaskInterface &task) noexcept { _local_queue.push_back(&task); }

    void push_back_remote(TaskInterface &task) noexcept { _remote_queue.push_back(&task); }

    void flush() noexcept;

    [[nodiscard]] TaskInterface *pop_front() { return _local_queue.pop_front(); }

private:
    queue::List<TaskInterface> _local_queue;
    alignas(64) queue::MPSC<TaskInterface> _remote_queue;
};

class TaskSquadSpawnTask final : public TaskInterface
{
public:
    TaskSquadSpawnTask(TaskSquad &squad) : _task_squad(squad) {}
    ~TaskSquadSpawnTask() override = default;
    TaskResult execute(std::uint16_t worker_id) override;

private:
    TaskSquad &_task_squad;
};
} // namespace mx::tasking
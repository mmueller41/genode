#pragma once

#include "annotation.h"
#include "config.h"
#include "priority.h"
#include "task_stack.h"
#include <bitset>
#include <cstdint>
#include <functional>
#include <mx/queue/list.h>
#include <mx/resource/ptr.h>
#include <variant>
#include <vector>

namespace mx::tasking {

class TaskInterface;

/**
 * The TaskResult is returned by every task to tell the
 * runtime what happens next. Possibilities are run a
 * successor task, remove the returning task or stop
 * the entire runtime.
 */
class TaskResult
{
public:
    /**
     * Let the runtime know that the given task
     * should be run as a successor of the current
     * task. The runtime will schedule that task.
     *
     * @param successor_task Task to succeed.
     * @return A TaskResult that tells the
     *         runtime to run the given task.
     */
    static TaskResult make_succeed(TaskInterface *successor_task) noexcept { return TaskResult{successor_task, false}; }

    /**
     * Let the runtime know that the given task
     * should be run as a successor of the current
     * task. The runtime will schedule that task.
     *
     * @param successor_task Task to succeed.
     * @return A TaskResult that tells the
     *         runtime to run the given task.
     */
    static TaskResult make_succeed(mx::resource::ptr resource) noexcept { return TaskResult{resource, false}; }

    /**
     * Let the runtime know that the given task
     * should be removed after (successfully)
     * finishing.
     *
     * @return A TaskResult that tells the
     *         runtime to remove the returning task.
     */
    static TaskResult make_remove() noexcept { return TaskResult{nullptr, true}; }

    /**
     * Let the runtime know that the given task
     * should be run as a successor of the current
     * task and the current task should be removed.
     *
     * @param successor_task Task to succeed.
     * @return A TaskResult that tells the runtime
     *         to run the given task and remove the
     *         returning task.
     */
    static TaskResult make_succeed_and_remove(TaskInterface *successor_task) noexcept
    {
        return TaskResult{successor_task, true};
    }

    /**
     * Nothing will happen
     *
     * @return An empty TaskResult.
     */
    static TaskResult make_null() noexcept { return {}; }

    /**
     * Let the runtime know to stop after
     * the returning task.
     *
     * @param worker_id Id of the current worker.
     * @param stop_network If set to true, the network server will also be stopped.
     * @return A TaskResult that tells the
     *         runtime to top.
     */
    static TaskResult make_stop(std::uint16_t worker_id, bool stop_network = true) noexcept;

    constexpr TaskResult() = default;
    ~TaskResult() = default;

    TaskResult &operator=(const TaskResult &) = default;

    explicit operator TaskInterface *() const noexcept { return _successor_task; }
    explicit operator mx::resource::ptr() const noexcept { return _resource; }

    [[nodiscard]] bool is_remove() const noexcept { return _remove_task; }
    [[nodiscard]] bool has_successor() const noexcept { return _successor_task != nullptr; }
    [[nodiscard]] bool has_resource() const noexcept { return static_cast<bool>(_resource); }

private:
    constexpr TaskResult(TaskInterface *successor_task, const bool remove) noexcept
        : _successor_task(successor_task), _remove_task(remove)
    {
    }
    constexpr TaskResult(const mx::resource::ptr resource, const bool remove) noexcept
        : _resource(resource), _remove_task(remove)
    {
    }
    TaskInterface *_successor_task{nullptr};
    mx::resource::ptr _resource;
    bool _remove_task{false};
};

/**
 * The task is the central execution unit of mxtasking.
 * Every task that should be executed has to derive
 * from this class.
 */
class TaskInterface
{
public:
    using channel = std::uint16_t;
    using node = std::uint8_t;

    constexpr TaskInterface() = default;
    virtual ~TaskInterface() = default;

    /**
     * Will be executed by a worker when the task gets CPU time.
     *
     * @param worker_id     Worker ID the task is executed on.
     * @return Pointer to the follow up task.
     */
    virtual TaskResult execute(std::uint16_t worker_id) = 0;

    /**
     * @return Trace Id of the task, that will be included into recordings to assign
     *          time ranges to specific tasks.
     */
    [[nodiscard]] virtual std::uint64_t trace_id() const noexcept { return 0U; }

    /**
     * @return The annotation of the task.
     */
    [[nodiscard]] const annotation &annotation() const noexcept { return _annotation; }

    /**
     * @return The annotation of the task.
     */
    [[nodiscard]] class annotation &annotation() noexcept { return _annotation; }

    /**
     * Annotate the task with a resource the task will work on.
     * The size identifies how many bytes will be prefetched.
     *
     * @param resource Pointer to the resource.
     * @param size  Size of the resource (that will be prefetched).
     */
    void annotate(const mx::resource::ptr resource_, const std::uint16_t size) noexcept
    {
        annotate(resource_, PrefetchSize::make(PrefetchDescriptor::PrefetchType::Temporal, size));
    }

    /**
     * Annotate the task with a resource the task will work on.
     * The object will be used for synchronization and prefetching.
     *
     * @param resource Pointer to the resource.
     * @param prefetch_hint  Mask for prefetching the resource.
     */
    void annotate(const mx::resource::ptr resource_, const PrefetchDescriptor descriptor) noexcept
    {
        annotate(resource_);
        annotate(PrefetchHint{descriptor, resource_});
    }

    /**
     * Annotate the task with a resource the task will work on.
     * The data object will be used for synchronization only.
     *
     * @param resource Pointer to the resource.
     */
    void annotate(const mx::resource::ptr resource_) noexcept { _annotation.set(resource_); }

    /**
     * Annotate the task with a prefetch hint that will be prefetched.
     *
     * @param prefetch_hint Hint for prefetching.
     */
    void annotate(const PrefetchHint prefetch_hint) noexcept { _annotation.set(prefetch_hint); }

    /**
     * Annotate the task with a desired channel the task should be executed on.
     *
     * @param worker_id ID of the channel.
     */
    void annotate(const std::uint16_t worker_id) noexcept { _annotation.set(worker_id); }

    /**
     * Annotate the task with a desired NUMA node id the task should executed on.
     *
     * @param node_id ID of the NUMA node.
     */
    void annotate(const std::uint8_t node_id) noexcept { _annotation.set(node_id); }

    /**
     * Annotate the task with a run priority (low, normal, high).
     *
     * @param priority_ Priority the task should run with.
     */
    void annotate(const priority priority_) noexcept { _annotation.set(priority_); }

    /**
     * Copy annotations from other task to this one.
     *
     * @param other Other task to copy annotations from.
     */
    void annotate(TaskInterface *other) noexcept { _annotation = other->_annotation; }

    /**
     * Copy annotation to this one.
     *
     * @param annotation
     */
    void annotate(const auto &annotation) noexcept { _annotation = annotation; }

    /**
     * Annotate the task to execute on a specific destination.
     *
     * @param execution_destination Destination to execute on.
     */
    void annotate(const annotation::execution_destination execution_destination) noexcept
    {
        _annotation.set(execution_destination);
    }

    /**
     * Annotate the task whether it is a reading or writing task.
     *
     * @param is_readonly True, when the task is read only (false by default).
     */
    void annotate(const annotation::access_intention access_intention) noexcept { _annotation.set(access_intention); }

    /**
     * @return Pointer to the next task in spawn queue.
     */
    [[nodiscard]] TaskInterface *next() const noexcept { return _next; }

    /**
     * Set the next task for scheduling.
     * @param next Task scheduled after this task.
     */
    void next(TaskInterface *next) noexcept { _next = next; }

private:
    /// Pointer for next task in queue.
    TaskInterface *_next{nullptr};

    /// Tasks annotations.
    class annotation _annotation
    {
    };
};

class LambdaTask : public TaskInterface
{
public:
    LambdaTask(std::function<TaskResult(std::uint16_t)> &&callback) noexcept : _callback(std::move(callback)) {}

    LambdaTask(std::function<void()> &&callback) noexcept
        : LambdaTask([callback = std::move(callback)](const std::uint16_t /*worker_id*/) {
              callback();
              return TaskResult::make_remove();
          })
    {
    }

    ~LambdaTask() noexcept override = default;

    TaskResult execute(std::uint16_t worker_id) override { return _callback(worker_id); }

private:
    std::function<TaskResult(std::uint16_t)> _callback;
};

class TaskLine : public TaskInterface
{
public:
    TaskLine() noexcept = default;
    ~TaskLine() noexcept override = default;

    TaskResult execute(std::uint16_t worker_id) override;

    void add(TaskInterface *task)
    {
        if (_next_task == nullptr)
        {
            _next_task = task;
            annotate(task);
        }
        else
        {
            _waiting_tasks.push_back(task);
        }
    }

    [[nodiscard]] bool empty() const noexcept { return _next_task == nullptr; }

private:
    TaskInterface *_next_task;
    queue::List<TaskInterface> _waiting_tasks;
};

class StopTaskingTask final : public TaskInterface
{
public:
    constexpr StopTaskingTask(const bool stop_network) noexcept : _stop_network(stop_network) {}
    ~StopTaskingTask() override = default;

    TaskResult execute(std::uint16_t worker_id) override;

private:
    const bool _stop_network;
};
} // namespace mx::tasking
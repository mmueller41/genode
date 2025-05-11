#pragma once

#include "prefetch_descriptor.h"
#include "priority.h"
#include <cstdint>
#include <mx/resource/ptr.h>
#include <variant>

namespace mx::tasking {

class TaskSquad;

/**
 * Container for metadata that can be annotated to every task.
 * The execution engine will use the annotation for scheduling
 * and synchronization of concurrent accesses to the same data
 * object, done by tasks.
 */
class Annotation
{
public:
    enum execution_destination : std::uint8_t
    {
        anywhere = 0U,
        local = 1U
    };
    enum access_intention : bool
    {
        readonly = true,
        write = false
    };
    enum resource_boundness : std::uint8_t
    {
        memory = 0U,
        compute = 1U,
        mixed = 2U
    };

    constexpr Annotation() noexcept = default;
    explicit constexpr Annotation(const std::uint16_t worker_id) noexcept : _destination(worker_id) {}
    explicit constexpr Annotation(const execution_destination destination) noexcept : _destination(destination) {}
    constexpr Annotation(const enum access_intention access_intention, const resource::ptr resource) noexcept
        : _access_intention(access_intention), _destination(resource)
    {
    }
    constexpr Annotation(const enum access_intention access_intention, const resource::ptr resource,
                         const PrefetchDescriptor prefetch_descriptor) noexcept
        : _access_intention(access_intention), _destination(resource),
          _prefetch_hint(PrefetchHint{prefetch_descriptor, resource})
    {
    }
    constexpr Annotation(const Annotation &) noexcept = default;
    constexpr Annotation(Annotation &&) noexcept = default;
    ~Annotation() = default;

    Annotation &operator=(const Annotation &) noexcept = default;
    Annotation &operator=(Annotation &&) noexcept = default;

    [[nodiscard]] bool is_readonly() const noexcept { return _access_intention == access_intention::readonly; }
    [[nodiscard]] priority priority() const noexcept { return _priority; }
    [[nodiscard]] enum resource_boundness resource_boundness() const noexcept { return _resource_boundness; }
    [[nodiscard]] std::uint16_t worker_id() const noexcept { return std::get<std::uint16_t>(_destination); }
    [[nodiscard]] std::uint8_t numa_node_id() const noexcept { return std::get<std::uint8_t>(_destination); }
    [[nodiscard]] resource::ptr resource() const noexcept { return std::get<resource::ptr>(_destination); }
    [[nodiscard]] bool has_worker_id() const noexcept { return std::holds_alternative<std::uint16_t>(_destination); }
    [[nodiscard]] bool has_numa_node_id() const noexcept { return std::holds_alternative<std::uint8_t>(_destination); }
    [[nodiscard]] bool has_resource() const noexcept { return std::holds_alternative<resource::ptr>(_destination); }
    [[nodiscard]] bool is_locally() const noexcept
    {
        return std::holds_alternative<enum execution_destination>(_destination) &&
               std::get<enum execution_destination>(_destination) == execution_destination::local;
    }
    [[nodiscard]] bool is_anywhere() const noexcept
    {
        return std::holds_alternative<enum execution_destination>(_destination) &&
               std::get<enum execution_destination>(_destination) == execution_destination::anywhere;
    }
    [[nodiscard]] bool has_prefetch_hint() const noexcept { return _prefetch_hint.empty() == false; }
    [[nodiscard]] PrefetchHint prefetch_hint() const noexcept { return _prefetch_hint; }
    [[nodiscard]] PrefetchHint &prefetch_hint() noexcept { return _prefetch_hint; }
    [[nodiscard]] std::uint16_t cycles() const noexcept { return _cycles; }

    void set(const enum access_intention access_intention) noexcept { _access_intention = access_intention; }
    void set(const enum priority priority) noexcept { _priority = priority; }
    void set(const enum resource_boundness resource_boundness) noexcept { _resource_boundness = resource_boundness; }
    void set(const std::uint16_t worker_id) noexcept { _destination = worker_id; }
    void set(const std::uint8_t numa_id) noexcept { _destination = numa_id; }
    void set(const resource::ptr resource) noexcept { _destination = resource; }
    void set(const enum execution_destination execution_destination) noexcept { _destination = execution_destination; }
    void set(const PrefetchDescriptor prefetch_descriptor, const resource::ptr object) noexcept
    {
        _prefetch_hint = PrefetchHint{prefetch_descriptor, object};
    }
    void set(const PrefetchHint prefetch_hint) noexcept { _prefetch_hint = prefetch_hint; }
    void cycles(const std::uint16_t cycles) noexcept { _cycles = cycles; }

    bool operator==(const Annotation &other) const noexcept
    {
        return _access_intention == other._access_intention && _priority == other._priority &&
               _destination == other._destination && _prefetch_hint == other._prefetch_hint;
    }

private:
    /// Access intention: Reading or writing the object?
    /// Per default, a task is annotated as a "writer".
    enum access_intention _access_intention
    {
        access_intention::write
    };

    /// Priority of a task. Low priority tasks will only be
    /// executed, whenever a worker has no higher-priorized
    /// tasks in his pool.
    enum priority _priority
    {
        priority::normal
    };

    enum resource_boundness _resource_boundness
    {
        resource_boundness::mixed
    };

    /// Cycles used for execution of this task.
    std::uint16_t _cycles{500U};

    /// Target the task will run on.
    /// The target can be a specific worker id, a NUMA node id,
    /// a specific data object (that may have a worker_id) or
    /// a hint like "local" or "anywhere".
    std::variant<std::uint16_t, std::uint8_t, resource::ptr, execution_destination> _destination{
        execution_destination::local};

    /// The prefetch hint is a data object that will be accessed
    /// by the task and a mask that identifies the cache lines,
    /// which should be prefetched.
    PrefetchHint _prefetch_hint{};
} __attribute__((packed));
} // namespace mx::tasking
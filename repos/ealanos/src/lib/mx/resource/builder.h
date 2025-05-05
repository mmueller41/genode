#pragma once
#include "annotation.h"
#include "ptr.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <mx/memory/global_heap.h>
#include <mx/memory/worker_local_dynamic_size_allocator.h>
#include <mx/tasking/config.h>
#include <mx/tasking/scheduler.h>
#include <mx/util/aligned_t.h>
#include <type_traits>
#include <utility>

namespace mx::resource {
/**
 * The Builder constructs and deletes data objects.
 * Besides, the Builder schedules data objects to
 * channels.
 */
class Builder
{
public:
    Builder(tasking::Scheduler &scheduler, memory::dynamic::local::Allocator &allocator) noexcept
        : _allocator(allocator), _scheduler(scheduler)
    {
    }

    ~Builder() noexcept = default;

    /**
     * Build a data object of given type with given
     * size and arguments. The hint defines the synchronization
     * requirements and affects scheduling.
     *
     * @param calling_worker_id Id of the calling worker for local allocation.
     * @param size Size of the data object.
     * @param hint  Hint for scheduling and synchronization.
     * @param arguments Arguments to the constructor.
     * @return Tagged pointer holding the synchronization, assigned channel and pointer.
     */
    template <typename T, typename... Args>
    ptr build(const std::uint16_t calling_worker_id, const std::size_t size, annotation &&annotation,
              Args &&...arguments) noexcept
    {
#ifndef NDEBUG
        if (annotation != synchronization::isolation_level::None &&
            (annotation != synchronization::isolation_level::Exclusive ||
             annotation != synchronization::protocol::Queue) &&
            (annotation != synchronization::isolation_level::Exclusive &&
             annotation != synchronization::protocol::Batched))
        {
            if constexpr (std::is_base_of<ResourceInterface, T>::value == false)
            {
                assert(false && "Type must be inherited from mx::resource::ResourceInterface");
            }
        }
#endif

        const auto synchronization_method = Builder::isolation_level_to_synchronization_primitive(annotation);

        const auto [mapped_worker_id, numa_node_id] = schedule(annotation);

        auto *resource = new (_allocator.allocate(calling_worker_id, numa_node_id, system::cache::line_size(), size))
            T(std::forward<Args>(arguments)...);

        if constexpr (std::is_base_of<ResourceInterface, T>::value)
        {
            switch (synchronization_method)
            {
            case synchronization::primitive::ExclusiveLatch:
            case synchronization::primitive::RestrictedTransactionalMemory:
                resource->initialize(ResourceInterface::SynchronizationType::Exclusive);
                break;
            case synchronization::primitive::ReaderWriterLatch:
                resource->initialize(ResourceInterface::SynchronizationType::SharedWrite);
                break;
            case synchronization::primitive::OLFIT:
            case synchronization::primitive::ScheduleWriter:
                resource->initialize(ResourceInterface::SynchronizationType::OLFIT);
                break;
            default:
                break;
            }
        }

        const auto resource_information = information{mapped_worker_id, synchronization_method};
        return ptr{resource, resource_information};
    }

    /**
     * Builds data resourced from an existing pointer.
     * The hint defines the synchronization
     * requirements and affects scheduling.
     * @param object
     * @param annotation  Hint for scheduling and synchronization.
     * @return Tagged pointer holding the synchronization, assigned channel and pointer.
     */
    template <typename T> ptr build(T *object, annotation &&annotation) noexcept
    {
#ifndef NDEBUG
        if (annotation != synchronization::isolation_level::None &&
            (annotation != synchronization::isolation_level::Exclusive ||
             annotation != synchronization::protocol::Queue) &&
            (annotation != synchronization::isolation_level::Exclusive &&
             annotation != synchronization::protocol::Batched))
        {
            if constexpr (std::is_base_of<ResourceInterface, T>::value == false)
            {
                assert(false && "Type must be inherited from mx::resource::ResourceInterface");
            }
        }
#endif

        const auto synchronization_method = Builder::isolation_level_to_synchronization_primitive(annotation);
        const auto [worker_id, _] = schedule(annotation);

        return ptr{object, information{worker_id, synchronization_method}};
    }

    /**
     * Destroys the given data object.
     * @param calling_worker_id Worker calling destroy for local free.
     * @param resource Tagged pointer to the data object.
     */
    template <typename T> void destroy(const std::uint16_t calling_worker_id, const ptr resource)
    {
        // TODO: Revoke usage prediction?
        if (resource != nullptr)
        {
            if constexpr (tasking::config::memory_reclamation() != tasking::config::None)
            {
                if (synchronization::is_optimistic(resource.synchronization_primitive()))
                {
                    _scheduler.epoch_manager().add_to_garbage_collection(resource.get<ResourceInterface>(),
                                                                         resource.worker_id());
                    return;
                }
            }

            // No need to reclaim memory.
            resource.get<T>()->~T();
            _allocator.free(calling_worker_id, resource.get<void>());
        }
    }

private:
    // Internal allocator for dynamic sized allocation.
    memory::dynamic::local::Allocator &_allocator;

    // Scheduler of MxTasking to get access to channels.
    tasking::Scheduler &_scheduler;

    // Next channel id for round-robin scheduling.
    alignas(64) std::atomic_uint16_t _round_robin_worker_id{0U};

    /**
     * Schedules the resource to a channel, affected by the given hint.
     *
     * @param annotation Hint for scheduling.
     * @return Pair of Channel and NUMA node IDs.
     */
    std::pair<std::uint16_t, std::uint8_t> schedule(const annotation &annotation);

    /**
     * Determines the best synchronization method based on
     * synchronization requirement.
     *
     * @param annotation Hint for choosing the primitive.
     * @return Chosen synchronization method.
     */
    static synchronization::primitive isolation_level_to_synchronization_primitive(
        const annotation &annotation) noexcept;
};
} // namespace mx::resource
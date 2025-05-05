#include "builder.h"
#include <mx/synchronization/primitive_matrix.h>

using namespace mx::resource;

std::pair<std::uint16_t, std::uint8_t> Builder::schedule(const resource::annotation &annotation)
{
    // Scheduling was done by the hint.
    if (annotation.has_worker_id())
    {
        this->_scheduler.predict_usage(annotation.worker_id(), annotation.access_frequency());
        return std::make_pair(annotation.worker_id(), this->_scheduler.numa_node_id(annotation.worker_id()));
    }

    // Schedule resources round robin to the channels.
    const auto count_worker = this->_scheduler.count_cores();
    auto worker_id = this->_round_robin_worker_id.fetch_add(1U, std::memory_order_relaxed) % count_worker;

    // If the chosen channel contains an excessive accessed resource, get another.
    if (count_worker > 2U && annotation.isolation_level() == synchronization::isolation_level::Exclusive &&
        this->_scheduler.has_excessive_usage_prediction(worker_id))
    {
        worker_id = this->_round_robin_worker_id.fetch_add(1U, std::memory_order_relaxed) % count_worker;
    }
    this->_scheduler.predict_usage(worker_id, annotation.access_frequency());

    // TODO: NUMA NODE ID is for worker, not channel.
    const auto numa_node_id =
        annotation.has_numa_node_id() ? annotation.numa_node_id() : this->_scheduler.numa_node_id(worker_id);

    return std::make_pair(worker_id, numa_node_id);
}

mx::synchronization::primitive Builder::isolation_level_to_synchronization_primitive(
    const annotation &annotation) noexcept
{
    // The developer did not define any fixed protocol for
    // synchronization; we choose one depending on the hints.
    if (annotation == synchronization::protocol::None)
    {
        return synchronization::PrimitiveMatrix::select_primitive(
            annotation.isolation_level(), annotation.access_frequency(), annotation.read_write_ratio());
    }

    // The developer hinted a specific protocol (latched, queued, ...)
    // and a relaxed isolation level.
    if (annotation == synchronization::isolation_level::ExclusiveWriter)
    {
        switch (annotation.preferred_protocol())
        {
        case synchronization::protocol::Latch:
            return synchronization::primitive::ReaderWriterLatch;
        case synchronization::protocol::OLFIT:
            return synchronization::primitive::OLFIT;
        case synchronization::protocol::RestrictedTransactionalMemory:
            return synchronization::primitive::RestrictedTransactionalMemory;
        default:
            return synchronization::primitive::ScheduleWriter;
        }
    }

    // The developer hinted a specific protocol (latched, queued, ...)
    // and a strict isolation level.
    if (annotation == synchronization::isolation_level::Exclusive)
    {
        switch (annotation.preferred_protocol())
        {
        case synchronization::protocol::Latch:
            return synchronization::primitive::ExclusiveLatch;
        case synchronization::protocol::Batched:
            return synchronization::primitive::Batched;
        case synchronization::protocol::RestrictedTransactionalMemory:
            return synchronization::primitive::RestrictedTransactionalMemory;
        default:
            return synchronization::primitive::ScheduleAll;
        }
    }

    return mx::synchronization::primitive::None;
}
#pragma once

#include <atomic>
#include <cstdint>
#include <mx/memory/reclamation/epoch_t.h>
#include <mx/synchronization/memory_transaction.h>
#include <mx/synchronization/optimistic_lock.h>
#include <mx/synchronization/rw_spinlock.h>
#include <mx/synchronization/spinlock.h>

namespace mx::resource {
/**
 * The resource interface represents resources that
 * needs to be synchronized by the tasking engine.
 * Supported synchronizations are:
 *  - Latches (Spinlock, R/W-lock)
 *  - Optimistic latches + memory reclamation
 */
class ResourceInterface
{
public:
    enum SynchronizationType : std::uint8_t
    {
        Exclusive,
        SharedRead,
        SharedWrite,
        Optimistic,
        OLFIT,
        RestrictedTransactionalMemory,
    };

    constexpr ResourceInterface() noexcept = default;
    ResourceInterface(const ResourceInterface &) = delete;
    ResourceInterface(ResourceInterface &&) = delete;
    virtual ~ResourceInterface() = default;

    void initialize(const SynchronizationType type) noexcept
    {
        switch (type)
        {
        case Exclusive:
        case RestrictedTransactionalMemory:
            _exclusive_latch.unlock();
            break;
        case SharedRead:
        case SharedWrite:
            _rw_latch.initialize();
            break;
        case Optimistic:
        case OLFIT:
            _optimistic_latch.initialize();
            break;
        }
    }

    /**
     * Called by the epoch manager on safe reclaiming this resource.
     */
    virtual void on_reclaim() = 0;

    /**
     * Set the next resource in garbage list.
     * @param next Next resource in garbage list.
     */
    void next(ResourceInterface *next) noexcept { _next_garbage = next; }

    /**
     * @return Next resource in garbage list.
     */
    [[nodiscard]] ResourceInterface *next() const noexcept { return _next_garbage; }

    /**
     * @return The current version of the resource.
     */
    [[nodiscard]] synchronization::OptimisticLock::version_t version() const noexcept
    {
        return _optimistic_latch.read_valid();
    }

    /**
     * Checks whether the given version is still valid.
     *
     * @param version Version to check.
     * @return True, when the version is valid.
     */
    [[nodiscard]] bool is_version_valid(const synchronization::OptimisticLock::version_t version) const noexcept
    {
        return _optimistic_latch.is_valid(version);
    }

    /**
     * Tries to acquire the optimistic latch.
     * @return True, when latch was acquired.
     */
    [[nodiscard]] bool try_acquire_optimistic_latch() noexcept { return _optimistic_latch.try_lock(); }

    /**
     * Set the epoch-timestamp this resource was removed.
     * @param epoch Epoch where this resource was removed.
     */
    void remove_epoch(const memory::reclamation::epoch_t epoch) noexcept { _remove_epoch = epoch; }

    /**
     * @return The epoch this resource was removed.
     */
    [[nodiscard]] memory::reclamation::epoch_t remove_epoch() const noexcept { return _remove_epoch; }

    template <SynchronizationType T> class scoped_latch
    {
    public:
        constexpr inline explicit scoped_latch(ResourceInterface *resource) noexcept : _resource(resource)
        {
            if constexpr (T == SynchronizationType::Exclusive)
            {
                _resource->_exclusive_latch.lock();
            }
            else if constexpr (T == SynchronizationType::SharedRead)
            {
                _resource->_rw_latch.lock_shared();
            }
            else if constexpr (T == SynchronizationType::SharedWrite)
            {
                _resource->_rw_latch.lock();
            }
            else if constexpr (T == SynchronizationType::Optimistic)
            {
                _resource->_optimistic_latch.lock<true>();
            }
            else if constexpr (T == SynchronizationType::OLFIT)
            {
                _resource->_optimistic_latch.lock<false>();
            }
            else if constexpr (T == SynchronizationType::RestrictedTransactionalMemory)
            {
                _is_transaction_used_latch = synchronization::MemoryTransaction::begin(_resource->_exclusive_latch);
            }
        }

        inline ~scoped_latch() noexcept
        {
            if constexpr (T == SynchronizationType::Exclusive)
            {
                _resource->_exclusive_latch.unlock();
            }
            else if constexpr (T == SynchronizationType::SharedRead)
            {
                _resource->_rw_latch.unlock_shared();
            }
            else if constexpr (T == SynchronizationType::SharedWrite)
            {
                _resource->_rw_latch.unlock();
            }
            else if constexpr (T == SynchronizationType::Optimistic || T == SynchronizationType::OLFIT)
            {
                _resource->_optimistic_latch.unlock();
            }
            else if constexpr (T == SynchronizationType::RestrictedTransactionalMemory)
            {
                synchronization::MemoryTransaction::end(_resource->_exclusive_latch, _is_transaction_used_latch);
            }
        }

    private:
        ResourceInterface *_resource;
        bool _is_transaction_used_latch;
    };

    using scoped_exclusive_latch = scoped_latch<SynchronizationType::Exclusive>;
    using scoped_optimistic_latch = scoped_latch<SynchronizationType::Optimistic>;
    using scoped_olfit_latch = scoped_latch<SynchronizationType::OLFIT>;
    template <bool WRITER>
    using scoped_rw_latch = scoped_latch<WRITER ? SynchronizationType::SharedWrite : SynchronizationType::SharedRead>;
    using scoped_transaction = scoped_latch<SynchronizationType::RestrictedTransactionalMemory>;

private:
    // Encapsulated synchronization primitives.
    union {
        synchronization::Spinlock _exclusive_latch;
        synchronization::RWSpinLock _rw_latch;
        synchronization::OptimisticLock _optimistic_latch;
    };

    // Epoch and Garbage management.
    memory::reclamation::epoch_t _remove_epoch{0U};
    ResourceInterface *_next_garbage{nullptr};
};
} // namespace mx::resource
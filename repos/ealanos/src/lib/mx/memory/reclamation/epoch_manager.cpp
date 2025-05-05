#include "epoch_manager.h"
#include <mx/queue/list.h>
#include <mx/system/cpu.h>
#include <mx/tasking/runtime.h>
#include <thread>

using namespace mx::memory::reclamation;

void EpochManager::enter_epoch_periodically()
{
    // Wait until the scheduler starts the system.
    while (this->_is_running == false)
    {
        system::builtin::pause();
    }

    // Enter new epochs and collect garbage periodically
    // while the system is running.
    while (this->_is_running)
    {
        // Enter new epoch.
        this->_global_epoch.fetch_add(1U);

        if constexpr (config::local_garbage_collection())
        {
            // Collect local garbage.
            // TODO: This might be buggy (even with cpu id, since threads could be interrupted within allocation)!
            const auto core_id = mx::system::cpu::core_id();
            for (auto worker_id = std::uint16_t(0U); worker_id < this->_count_channels; ++worker_id)
            {
                auto *garbage_task =
                    mx::tasking::runtime::new_task<ReclaimEpochGarbageTask>(core_id, *this, this->_allocator);
                garbage_task->annotate(worker_id);
                mx::tasking::runtime::spawn(*garbage_task);
            }
        }
        else
        {
            // Collect global garbage of finished epochs.
            this->reclaim_epoch_garbage();
        }

        // Wait some time until next epoch.
        std::this_thread::sleep_until(std::chrono::system_clock::now() + config::epoch_interval());
    }
}

void EpochManager::reclaim_epoch_garbage() noexcept
{
    // Items logically removed in an epoch leq than
    // this epoch can be removed physically.
    const auto min_epoch = this->min_local_epoch();

    // Items that could not be physically removed in this epoch
    // and therefore have to be scheduled to the next one.
    queue::List<resource::ResourceInterface> deferred_resources{};

    resource::ResourceInterface *resource;
    while ((resource = reinterpret_cast<resource::ResourceInterface *>(this->_global_garbage_queue.pop_front())) !=
           nullptr)
    {
        if (resource->remove_epoch() < min_epoch)
        {
            resource->on_reclaim();
            this->_allocator.free(static_cast<void *>(resource));
        }
        else
        {
            deferred_resources.push_back(resource);
        }
    }

    // Resources that could not be deleted physically
    // need to be deleted in next epochs.
    if (deferred_resources.empty() == false)
    {
        this->_global_garbage_queue.push_back(deferred_resources.begin(), deferred_resources.end());
    }
}

void EpochManager::reclaim_all() noexcept
{
    if constexpr (config::local_garbage_collection())
    {
        for (auto worker_id = 0U; worker_id < this->_count_channels; ++worker_id)
        {
            resource::ResourceInterface *resource;
            while ((resource = reinterpret_cast<resource::ResourceInterface *>(
                        this->_local_garbage_queues[worker_id].value().pop_front())) != nullptr)
            {
                resource->on_reclaim();
                this->_allocator.free(static_cast<void *>(resource));
            }
        }
    }
    else
    {
        resource::ResourceInterface *resource;
        while ((resource = reinterpret_cast<resource::ResourceInterface *>(this->_global_garbage_queue.pop_front())) !=
               nullptr)
        {
            resource->on_reclaim();
            this->_allocator.free(static_cast<void *>(resource));
        }
    }
}

void EpochManager::reset() noexcept
{
    if (this->_allocator.is_free())
    {
        this->_global_epoch.store(0U);
        for (auto worker_id = 0U; worker_id < tasking::config::max_cores(); ++worker_id)
        {
            _local_epochs[worker_id] = std::numeric_limits<epoch_t>::max();
        }
    }
}

mx::tasking::TaskResult ReclaimEpochGarbageTask::execute(const std::uint16_t worker_id)
{
    // Items logically removed in an epoch leq than
    // this epoch can be removed physically.
    const auto min_epoch = this->_epoch_manager.min_local_epoch();

    // Items that could not be physically removed in this epoch
    // and therefore have to be scheduled to the next one.
    queue::List<resource::ResourceInterface> deferred_resources{};

    // Queue with channel-local garbage.
    auto &garbage_queue = this->_epoch_manager.local_garbage(worker_id);

    resource::ResourceInterface *resource;
    while ((resource = reinterpret_cast<resource::ResourceInterface *>(garbage_queue.pop_front())) != nullptr)
    {
        if (resource->remove_epoch() < min_epoch)
        {
            resource->on_reclaim();
            this->_allocator.free(static_cast<void *>(resource));
        }
        else
        {
            deferred_resources.push_back(resource);
        }
    }

    // Resources that could not be deleted physically
    // need to be deleted in next epochs.
    if (deferred_resources.empty() == false)
    {
        garbage_queue.push_back(deferred_resources.begin(), deferred_resources.end());
    }

    return tasking::TaskResult::make_remove();
}

#pragma once

#include <atomic>
#include <cstdint>
#include <mx/system/builtin.h>

namespace mx::synchronization {
/**
 * Simple spinlock for mutual exclusion.
 */
class Spinlock
{
public:
    constexpr Spinlock() noexcept = default;
    ~Spinlock() = default;

    /**
     * Locks the spinlock by spinning until it is lockable.
     */
    void lock() noexcept
    {
        do
        {
            while (_flag)
            {
                system::builtin::pause();
            }

            if (try_lock())
            {
                return;
            }
        } while (true);
    }

    /**
     * Try to lock the lock.
     * @return True, when successfully locked.
     */
    bool try_lock() noexcept
    {
        bool expected = false;
        return __atomic_compare_exchange_n(&_flag, &expected, true, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

    /**
     * Unlocks the spinlock.
     */
    void unlock() noexcept { __atomic_store_n(&_flag, false, __ATOMIC_SEQ_CST); }

    /**
     * @return True, if the lock is in use.
     */
    [[nodiscard]] bool is_locked() const noexcept { return __atomic_load_n(&_flag, __ATOMIC_RELAXED); }

private:
    bool _flag;
};
} // namespace mx::synchronization

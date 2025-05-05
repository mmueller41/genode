#pragma once

#include "spinlock.h"
#ifdef USE_RTM
#include <immintrin.h>
#endif 
namespace mx::synchronization {
class MemoryTransaction
{
private:
    [[nodiscard]] static constexpr auto max_tries() { return 10U; }
    [[nodiscard]] static constexpr auto abort_because_locked_code() { return 0xFF; }

public:
    [[nodiscard]] static bool begin(Spinlock &latch) noexcept
    {
#ifdef USE_RTM
        auto retries = 0U;

        do
        {
            const auto status = _xbegin();
            if (status == _XBEGIN_STARTED)
            {
                if (latch.is_locked() == false)
                {
                    /// Transaction was started successfully
                    ///  and the latch was not acquired by another thread.
                    return false;
                }

                /// Transaction was started, but lock was acquired from another thread.
                _xabort(abort_because_locked_code());
            }
            else if (status & _XABORT_EXPLICIT)
            {
                if (_XABORT_CODE(status) == abort_because_locked_code() && (status & _XABORT_NESTED) == false)
                {
                    /// The transaction was aborted because another thread
                    /// holds the lock. Wait until the thread releases
                    /// the lock.
                    while (latch.is_locked())
                    {
                        mx::system::builtin::pause();
                    }
                }
                else if ((status & _XABORT_RETRY) == false)
                {
                    /// The system tells us, that we should not retry.
                    /// Hence, acquire the latch.
                    goto acquire_fallback_lock;
                }
            }
        } while (++retries <= max_tries());

    acquire_fallback_lock:
        latch.lock();
        return true;
#else
        latch.lock();
        return true;
#endif
    }

    static void end(Spinlock &latch, [[maybe_unused]] const bool has_locked) noexcept
    {
#ifdef USE_RTM
        if (has_locked == false)
        {
            _xend();
        }
        else
        {
            latch.unlock();
        }
#else
        latch.unlock();
#endif
    }
};
} // namespace mx::synchronization
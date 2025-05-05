#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include <mx/system/builtin.h>
#include <mx/tasking/config.h>

namespace mx::synchronization {
class OptimisticLock
{
public:
    using version_t = std::uint32_t;

    constexpr OptimisticLock() = default;
    ~OptimisticLock() = default;

    void initialize() { _version = 0b100; }

    /**
     * Guarantees to read a valid version by blocking until
     * the version is not locked.
     * @return The current version.
     */
    [[nodiscard]] version_t read_valid() const noexcept
    {
        auto version = __atomic_load_n(&_version, __ATOMIC_SEQ_CST);
        while (OptimisticLock::is_locked(version))
        {
            system::builtin::pause();
            version = __atomic_load_n(&_version, __ATOMIC_SEQ_CST);
        }
        return version;
    }

    /**
     * Validates the version.
     *
     * @param version The version to validate.
     * @return True, if the version is valid.
     */
    [[nodiscard]] bool is_valid(const version_t version) const noexcept
    {
        return version == __atomic_load_n(&_version, __ATOMIC_SEQ_CST);
    }

    /**
     * Tries to acquire the lock.
     * @return True, when lock was acquired.
     */
    [[nodiscard]] bool try_lock() noexcept
    {
        auto version = read_valid();

        return __atomic_compare_exchange_n(&_version, &version, version + 0b10, false, __ATOMIC_SEQ_CST,
                                           __ATOMIC_SEQ_CST);
    }

    /**
     * Waits until the lock is successfully acquired.
     */
    template <bool SINGLE_WRITER> void lock() noexcept
    {
        if constexpr (SINGLE_WRITER)
        {
            __atomic_fetch_add(&_version, 0b10, __ATOMIC_SEQ_CST);
        }
        else
        {
            auto tries = std::uint64_t{1U};
            while (this->try_lock() == false)
            {
                const auto wait = tries++;
                for (auto i = 0U; i < wait * 32U; ++i)
                {
                    system::builtin::pause();
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
            }
        }
    }

    /**
     * Unlocks the version lock.
     */
    void unlock() noexcept { __atomic_fetch_add(&_version, 0b10, __ATOMIC_SEQ_CST); }

private:
    version_t _version;

    [[nodiscard]] static bool is_locked(const version_t version) noexcept { return (version & 0b10) == 0b10; }
};
} // namespace mx::synchronization
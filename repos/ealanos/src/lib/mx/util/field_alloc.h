#pragma once
#include "random.h"
#include <atomic>
#include <base/log.h>

namespace mx::util {
    template <unsigned C> class Field_Allocator;
}

template <unsigned C> 
class mx::util::Field_Allocator
{
    private:
        struct field {
            alignas(64) std::atomic<bool> reserved{false};
        };

        alignas(64) struct field _fields[C]{};
        alignas(64) std::size_t _count;
        std::atomic<std::size_t> free_fields;

    public:
        Field_Allocator(std::size_t count) : _count(count), free_fields(count) {}

        std::size_t alloc_randomly(unsigned offset, unsigned limit) {
            if (free_fields.load(std::memory_order_relaxed) <= 0)
                return 0;

            //mx::util::Random rng(Genode::Trace::timestamp());

            //Genode::log("Searching queue");
            std::size_t candidate = offset;
       //      +rng.next(limit);

            if (candidate > (offset + limit))
                return 0;

            if (candidate == 0)
                return 0;

            for (; candidate < offset + limit; candidate++) {
                bool expect = false;
                bool success = _fields[candidate].reserved.compare_exchange_strong(expect, true, std::memory_order_acquire, std::memory_order_relaxed);
                if (success) {
                    free_fields.fetch_sub(1);
                    return candidate;
                }
            }
            return 0;
        }

        void release(std::size_t field) {
            _fields[field].reserved.store(false);
            free_fields.fetch_add(1);
        }
};
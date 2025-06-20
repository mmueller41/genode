#pragma once

#include <mx/synchronization/lock_guard.h>
#include <mx/synchronization/optimistic_lock.h>

namespace Mxip {
    extern mx::synchronization::OptimisticLock mxip_lock;
}
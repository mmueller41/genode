#include "environment.h"
#include <mx/util/core_set.h>

using namespace mx::system;

void Environment::set_cores(mx::util::core_set *core_set)
{
    system::Environment::get_instance()._coreset = core_set;
}

mx::util::core_set &Environment::cores()
{
    return *Environment::get_instance()._coreset;
}

unsigned long mx::system::Environment::tsc_freq_khz;
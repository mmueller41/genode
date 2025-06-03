#include "core_set.h"
#include <algorithm>
#include <mx/system/topology.h>
#include <mx/tasking/config.h>
#include <numeric>
#include <vector>
#include <base/log.h>

using namespace mx::util;

core_set core_set::build(std::uint16_t cores, const Order order)
{
	cores = std::min(cores, std::min(std::uint16_t(tasking::config::max_cores()),
	                                 system::topology::count_cores()));

    core_set core_set;
    if (order == Ascending)
    {
        for (auto i = 0U; i < cores; ++i)
        {
            core_set.emplace_back(i);
        }
    }
    else if (order == NUMAAware)
    {
        std::vector<std::uint16_t> cores_to_sort(system::topology::count_cores());
        std::iota(cores_to_sort.begin(), cores_to_sort.end(), 0U);
        std::sort(cores_to_sort.begin(), cores_to_sort.end(),
                  [](const std::uint16_t &left, const std::uint16_t &right) {
                      const auto left_node = system::topology::node_id(left);
                      const auto right_node = system::topology::node_id(right);
                      if (left_node == right_node)
                      {
                          return left < right;
                      }

                      return left_node < right_node;
                  });
        for (auto i = 0U; i < cores; ++i)
        {
            core_set.emplace_back(cores_to_sort[i]);
        }
    }

    return core_set;
}

core_set core_set::build(std::uint64_t *core_mask, std::uint16_t count)
{
    core_set core_set;
    for (int c = 0; c < count; ++count)
    {
        std::bitset<tasking::config::max_cores()> mask{core_mask[c]};
        long core = 0;

        while ((core = util::bit_scan_forward(mask.to_ulong())) != -1) {
            mask.reset(core);
            core_set.emplace_back(core);
        }
    }
}

namespace mx::util {
std::ostream &operator<<(std::ostream &stream, const core_set &core_set)
{
    for (auto i = 0U; i < core_set.size(); i++)
    {
        if (i > 0U)
        {
            stream << " ";
        }
        stream << core_set[i];
    }
    return stream << std::flush;
}
} // namespace mx::util

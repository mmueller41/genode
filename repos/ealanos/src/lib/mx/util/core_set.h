#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <mx/memory/config.h>
#include <mx/system/cpu.h>
#include <mx/tasking/config.h>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace mx::util {
/**
 * The core set is used to identify cores included into MxTasking runtime.
 * Naively, we would just specify the number of used cores; this would reach
 * the limit when reordering the core ids (e.g. to include all cores of a
 * single NUMA region).
 */
class core_set
{
    friend std::ostream &operator<<(std::ostream &stream, const core_set &ore_set);

public:
    enum Order
    {
        Ascending,
        NUMAAware
    };

    /**
     * Builds the core set for a fixed number of cores and specified ordering.
     * @param channels Number of channels.
     * @param cores Number of cores.
     * @param order The order can be "Ascending" (for using the systems order) or "NUMA Aware".
     * @return
     */
    [[nodiscard]] static core_set build(std::uint16_t cores, Order order = Ascending);

    /**
     * Builds the core set for all cores in the system and specified ordering.
     * @param order The order can be "Ascending" (for using the systems order) or "NUMA Aware".
     * @return
     */
    [[nodiscard]] static core_set build(Order order = Ascending)
    {
        return core_set::build(system::cpu::count_cores(), order);
    }

    constexpr core_set() noexcept = default;

    core_set(std::initializer_list<std::uint16_t> &&core_ids) noexcept
    {
        for (const auto core_id : core_ids)
        {
            emplace_back(core_id);
        }
    }
    ~core_set() noexcept = default;

    core_set &operator=(const core_set &other) noexcept = default;

    std::uint16_t operator[](const std::uint16_t index) const noexcept { return _worker_core_map[index]; }
    [[nodiscard]] std::uint16_t front() const { return _worker_core_map.front(); }
    [[nodiscard]] std::uint16_t back() const { return _worker_core_map.back(); }

    explicit operator bool() const noexcept { return _count_cores > 0U; }

    /**
     * @return Number of included cores.
     */
    [[nodiscard]] std::uint16_t count_cores() const noexcept { return _count_cores; }

    /**
     * @return Number of included NUMA regions.
     */
    [[nodiscard]] std::uint16_t numa_nodes() const noexcept { return _numa_nodes.count(); }

    /**
     * NUMA node id of the given channel.
     * @param index Channel.
     * @return NUMA node id.
     */
    [[nodiscard]] std::uint8_t numa_node_id(const std::uint16_t index) const noexcept
    {
        return system::cpu::node_id(_worker_core_map[index]);
    }

    /**
     * @return Highest included core identifier.
     */
    [[nodiscard]] std::uint16_t max_core_id() const noexcept
    {
        return *std::max_element(_worker_core_map.cbegin(), _worker_core_map.cbegin() + _count_cores);
    }

    bool operator==(const core_set &other) const noexcept
    {
        return _worker_core_map == other._worker_core_map && _count_cores == other._count_cores &&
               _numa_nodes == other._numa_nodes;
    }

    bool operator!=(const core_set &other) const noexcept
    {
        return _worker_core_map != other._worker_core_map || _count_cores != other._count_cores ||
               _numa_nodes != other._numa_nodes;
    }

    /**
     * @param numa_node_id NUMA node identifier.
     * @return True, when the NUMA region is represented in the core set.
     */
    [[nodiscard]] bool has_core_of_numa_node(const std::uint8_t numa_node_id) const noexcept
    {
        return _numa_nodes.test(numa_node_id);
    }

    [[nodiscard]] auto begin() const noexcept { return _worker_core_map.begin(); }
    [[nodiscard]] auto end() const noexcept { return _worker_core_map.begin() + _count_cores; }

    [[nodiscard]] std::string to_string() const noexcept
    {
        std::stringstream stream;
        stream << *this;
        return stream.str();
    }

private:
    // Maps from worker id (0..N) to core id.
    std::array<std::uint16_t, tasking::config::max_cores()> _worker_core_map{0U};

    // Number of cores in the set.
    std::uint16_t _count_cores{0U};

    // Bitvector for represented NUMA regions.
    std::bitset<memory::config::max_numa_nodes()> _numa_nodes{0U};

    /**
     * Add a core to the core set.
     * @param core_identifier Logical identifier of the core.
     */
    void emplace_back(const std::uint16_t core_identifier) noexcept
    {
        const auto worker_id = _count_cores++;

        _worker_core_map[worker_id] = core_identifier;
        _numa_nodes[system::cpu::node_id(core_identifier)] = true;
    }

    static void sort_by_numa(std::vector<std::uint16_t> &core_ids);
};
} // namespace mx::util
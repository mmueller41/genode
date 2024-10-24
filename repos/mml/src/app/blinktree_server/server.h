#pragma once

#include <db/index/blinktree/b_link_tree.h>
#include <mx/util/core_set.h>

namespace application::blinktree_server {
class Server
{
public:
    Server(std::uint64_t port, mx::util::core_set&& cores, std::uint16_t prefetch_distance, mx::synchronization::isolation_level node_isolation_level, mx::synchronization::protocol preferred_synchronization_method);

    void run();
private:
    const std::uint64_t _port;

    const std::uint16_t _prefetch_distance;

    /// Cores.
    mx::util::core_set _cores;

    // The synchronization mechanism to use for tree nodes.
    const mx::synchronization::isolation_level _node_isolation_level;

    // Preferred synchronization method.
    const mx::synchronization::protocol _preferred_synchronization_method;

    /// Tree.
    std::unique_ptr<db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>> _tree;
};
}
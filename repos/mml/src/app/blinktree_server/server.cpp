#include "server.h"
#include "network/server.h"
#include <iostream>

using namespace application::blinktree_server;

Server::Server(const std::uint64_t port, mx::util::core_set &&cores, const std::uint16_t prefetch_distance, const mx::synchronization::isolation_level node_isolation_level, const mx::synchronization::protocol preferred_synchronization_method)
    : _port(port), _cores(std::move(cores)), _prefetch_distance(prefetch_distance), _node_isolation_level(node_isolation_level), _preferred_synchronization_method(preferred_synchronization_method)
{
}

void Server::run()
{
    network::Server* server;

    mx::tasking::runtime::init(this->_cores, this->_prefetch_distance, /* use mx tasking's task allocator*/  false); 

    this->_tree = std::make_unique<db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>>(
        this->_node_isolation_level, this->_preferred_synchronization_method);

    server = new network::Server{this->_port, mx::tasking::runtime::channels()};

    std::cout << "Waiting for requests on port :" << this->_port << std::endl;
    auto network_thread = std::thread{[server, tree = this->_tree.get()]() {
        server->listen(tree);
    }};
    mx::tasking::runtime::start_and_wait();

    //network_thread.join();

    delete server;
}
#include "server.h"
#include "network/server.h"
#include <iostream>
#include <mx/system/environment.h>
#include <base/heap.h>
#include <timer_session/connection.h>

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

    Libc::Env &env = mx::system::Environment::env();

    static mx::memory::dynamic::Allocator *alloc = new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(mx::memory::dynamic::Allocator))) mx::memory::dynamic::Allocator();

    static Timer::Connection timer{env};

    static Genode::Heap _alloc{env.ram(), env.rm()};

    Mxip::mxip_init(*alloc, timer);
    server = new network::Server{env, this->_port, mx::tasking::runtime::channels(), timer, _alloc};

    std::cout << "Waiting for requests on port :" << this->_port << std::endl;
    auto network_thread = std::thread{[server, tree = this->_tree.get()]() {
        server->listen(tree);
    }};
    mx::tasking::runtime::start_and_wait();



    network_thread.join();


    //delete server;
}
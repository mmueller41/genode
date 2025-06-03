
#include "ealanos/memory/hamstraaja.h"
#include "mx/memory/global_heap.h"
#include "mx/system/topology.h"
#include "mxnic_netif.h"
#include "server.h"
#include <algorithm>
#include <base/heap.h>
#include <iostream>
#include <mx/system/environment.h>
#include <mx/util/core_set.h>
#include <mxip/genode_init.h>
#include <timer_session/connection.h>
#include <libc/component.h>

namespace application::echo_server
{
	class Server
	{
		public:

			Server(std::uint64_t port, mx::util::core_set &&cores, std::uint16_t prefetch_distance,
			       mx::synchronization::isolation_level node_isolation_level,
			       mx::synchronization::protocol        preferred_synchronization_method);

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
	};
} // namespace application::echo_server

using namespace application::echo_server;

Server::Server(const std::uint64_t port, mx::util::core_set &&cores,
               const std::uint16_t                        prefetch_distance,
               const mx::synchronization::isolation_level node_isolation_level,
               const mx::synchronization::protocol        preferred_synchronization_method)
	: _port(port), _cores(std::move(cores)), _prefetch_distance(prefetch_distance),
	  _node_isolation_level(node_isolation_level),
	  _preferred_synchronization_method(preferred_synchronization_method)
{
}

void Server::run()
{
	network::Server *server;

	Libc::Env &env = mx::system::Environment::env();
	mx::tasking::runtime::init(env, this->_cores, this->_prefetch_distance,
	                           /* use mx tasking's task allocator*/ false);

	static mx::memory::dynamic::Allocator *alloc = new (
		mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(mx::memory::dynamic::Allocator)))
		mx::memory::dynamic::Allocator();

	static Timer::Connection timer{env};

	static Genode::Heap _alloc{env.ram(), env.rm()};

	mx::memory::GlobalHeap::_alloc->reserve_superblocks(64, 128, 6);
	
	Mxip::mxip_init(*mx::memory::GlobalHeap::_alloc, timer);
	Ealan::Memory::Hamstraaja<128, 4 * 4096> *task_alloc =
		new (_alloc) Ealan::Memory::Hamstraaja<128, 4 * 4096>(env.pd(), env.rm());
	task_alloc->reserve_superblocks(48, 128, 6);
	Mxip::Nic_netif::Payload_allocator *palloc = new (_alloc) Mxip::Nic_netif::Payload_allocator(env.pd(), env.rm());
	server = new network::Server{env, this->_port, mx::tasking::runtime::channels(), timer, _alloc, task_alloc, palloc};

	std::cout << "Waiting for requests on port :" << this->_port << std::endl;
	auto network_thread = std::thread{[server]() {
		server->listen();
	}};

	std::cout << "Task sizes are: " << std::endl;
	std::cout << "MxIP::Receive_task: " << sizeof(Mxip::Receive_task) << std::endl;
	std::cout << "MxIP::Tx_ready_task: " << sizeof(Mxip::Tx_ready_task) << std::endl;
	std::cout << "MxIP::Link_state_task: " << sizeof(Mxip::Link_state_task) << std::endl;

	std::cout << "Server receive task: " << sizeof(network::ReceiveTask) << std::endl;
	mx::tasking::runtime::start_and_wait();

	network_thread.join();

	// delete server;
}

void Libc::Component::construct(Libc::Env &env)
{
	mx::system::Environment::set_env(&env);

	auto sys_cores = mx::util::core_set::build(64);
	mx::system::Environment::set_cores(&sys_cores);
	Libc::with_libc([&]() {
        auto cores = mx::util::core_set::build(64);


		auto *server = new Server(12345, std::move(cores), 3,
		                          mx::synchronization::isolation_level::ExclusiveWriter,
		                          mx::synchronization::protocol::OLFIT);

		server->run();
	});
}

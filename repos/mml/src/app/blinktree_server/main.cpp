#include "benchmark.h"
#include <argparse.hpp>
#include <benchmark/cores.h>
#include <mx/system/environment.h>
#include <mx/tasking/runtime.h>
#include <mx/util/core_set.h>
#include <tuple>
#include "server.h"
#include <cstring>
#include <cstdio>

/* Genode includes */
#include <libc/component.h>

using namespace application::blinktree_server;

/**
 * Instantiates the BLink-Tree server with CLI arguments.
 * @param count_arguments Number of CLI arguments.
 * @param arguments Arguments itself.
 *
 * @return Instance of the server.
 */
std::tuple<Server *, std::uint16_t, bool> create_server(int count_arguments, char **arguments);

/**
 * Starts the server.
 *
 * @param count_arguments Number of CLI arguments.
 * @param arguments Arguments itself.
 *
 * @return Return code of the application.
 */
int bt_main(int count_arguments, char **arguments)
{
    if (mx::system::Environment::is_numa_balancing_enabled())
    {
        std::cout << "[Warn] NUMA balancing may be enabled, set '/proc/sys/kernel/numa_balancing' to '0'" << std::endl;
    }

    auto [server, prefetch_distance, use_system_allocator] = create_server(count_arguments, arguments);

    if (server != nullptr)
    {
        /// Wait for the server to finish.
        server->run();

        delete server;
    }

    return 0;
}

std::tuple<Server *, std::uint16_t, bool> create_server(int count_arguments, char **arguments)
{
    /*
    // Set up arguments.
    argparse::ArgumentParser argument_parser("blinktree_server");
    argument_parser.add_argument("cores")
        .help("Number of cores to use.")
        .default_value(std::uint16_t(1))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("--port")
        .help("Port of the server")
        .default_value(std::uint64_t(12345))
        .action([](const std::string &value) { return std::uint64_t(std::stoi(value)); });
    argument_parser.add_argument("-sco", "--system-core-order")
        .help("Use systems core order. If not, cores are ordered by node id (should be preferred).")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--exclusive")
        .help("Are all node accesses exclusive?")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--latched")
        .help("Prefer latch for synchronization?")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--olfit")
        .help("Prefer OLFIT for synchronization?")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("--sync4me")
        .help("Let the tasking layer decide the synchronization primitive.")
        .implicit_value(true)
        .default_value(false);
    argument_parser.add_argument("-pd", "--prefetch-distance")
        .help("Distance of prefetched data objects (0 = disable prefetching).")
        .default_value(std::uint16_t(0))
        .action([](const std::string &value) { return std::uint16_t(std::stoi(value)); });
    argument_parser.add_argument("--system-allocator")
        .help("Use the systems malloc interface to allocate tasks (default disabled).")
        .implicit_value(true)
        .default_value(false);

    // Parse arguments.
    try
    {
        argument_parser.parse_args(count_arguments, arguments);
    }
    catch (std::runtime_error &e)
    {
        std::cout << argument_parser << std::endl;
        return {nullptr, 0U, false};
    }

    auto order =
        argument_parser.get<bool>("-sco") ? mx::util::core_set::Order::Ascending : mx::util::core_set::Order::NUMAAware;
    auto cores = mx::util::core_set::build(argument_parser.get<std::uint16_t>("cores")-1, order);
    const auto isolation_level = argument_parser.get<bool>("--exclusive")
                                     ? mx::synchronization::isolation_level::Exclusive
                                     : mx::synchronization::isolation_level::ExclusiveWriter;
    auto preferred_synchronization_method = mx::synchronization::protocol::Queue;
    if (argument_parser.get<bool>("--latched"))
    {
        preferred_synchronization_method = mx::synchronization::protocol::Latch;
    }
    else if (argument_parser.get<bool>("--olfit"))
    {
        preferred_synchronization_method = mx::synchronization::protocol::OLFIT;
    }
    else if (argument_parser.get<bool>("--sync4me"))
    {
        preferred_synchronization_method = mx::synchronization::protocol::None;
    }
    */
    // Create the benchmark.
    //auto *server = new Server(argument_parser.get<std::uint64_t>("--port"), std::move(cores), argument_parser.get<std::uint16_t>("-pd"), isolation_level, preferred_synchronization_method);

    auto cores = mx::util::core_set::build(64);

    auto *server = new Server(12345, std::move(cores), 3, mx::synchronization::isolation_level::ExclusiveWriter, mx::synchronization::protocol::OLFIT);

    return {server, 3, false};
    // return {server, argument_parser.get<std::uint16_t>("-pd"), argument_parser.get<bool>("--system-allocator")};
}

void Libc::Component::construct(Libc::Env &env) {

    mx::system::Environment::set_env(&env);

    auto sys_cores = mx::util::core_set::build(64);
    mx::system::Environment::set_cores(&sys_cores);

    mx::memory::GlobalHeap::myself();
    std::uint16_t cores = 64;
     //env.cpu().affinity_space().total();

    char cores_arg[10];
    sprintf(cores_arg, "%d", cores);

    char *args[] = {"blinktree_server", cores_arg};

    Libc::with_libc([&]()
                    { 
                        std::cout << "Starting B-link tree server" << std::endl;
                        bt_main(2, args); 
                    });
}
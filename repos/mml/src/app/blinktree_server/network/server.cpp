#include "server.h"
#include <limits>
#include <mx/tasking/runtime.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <db/index/blinktree/lookup_task.h>
#include <db/index/blinktree/insert_value_task.h>
#include <db/index/blinktree/update_task.h>
#include <mx/system/topology.h>

using namespace application::blinktree_server::network;

mx::tasking::TaskResult RequestTask::execute(const std::uint16_t core_id, const std::uint16_t channel_id)
{
    mx::tasking::TaskInterface* request_task;

    if (this->_type == Type::Insert)
    {
        request_task = mx::tasking::runtime::new_task<
            db::index::blinktree::InsertValueTask<std::uint64_t, std::int64_t, ResponseHandler>>(
            core_id, this->_key, this->_value, this->_tree, this->_response_handler);

        request_task->annotate(this->_tree->root(), db::index::blinktree::config::node_size() / 4U);
        request_task->is_readonly(true);
    }
    else if (this->_type == Type::Lookup)
    {
        request_task = mx::tasking::runtime::new_task<
            db::index::blinktree::LookupTask<std::uint64_t, std::int64_t, ResponseHandler>>(
            core_id, this->_key, this->_response_handler);

        request_task->annotate(this->_tree->root(), db::index::blinktree::config::node_size() / 4U);
        request_task->is_readonly(true);
    }
    else  if(this->_type == Type::Update)
    {
        request_task = mx::tasking::runtime::new_task<
            db::index::blinktree::UpdateTask<std::uint64_t, std::int64_t, ResponseHandler>>(
            core_id, this->_key, this->_value, this->_response_handler);

        request_task->annotate(this->_tree->root(), db::index::blinktree::config::node_size() / 4U);
        request_task->is_readonly(true);
    }
    else
    {
        this->_tree->check();
        this->_tree->print_statistics();
        return mx::tasking::TaskResult::make_null();
    }

    return mx::tasking::TaskResult::make_succeed(request_task);
}

void ResponseHandler::inserted(const std::uint16_t /*core_id*/, const std::uint64_t key, const std::int64_t /*value*/)
{
    _server-> send(_client_id, std::to_string(key));
}

void ResponseHandler::updated(const std::uint16_t /*core_id*/, const std::uint64_t key, const std::int64_t /*value*/)
{
    _server-> send(_client_id, std::to_string(key));
}

void ResponseHandler::removed(const std::uint16_t /*core_id*/, const std::uint64_t key)
{
    _server-> send(_client_id, std::to_string(key));
}

void ResponseHandler::found(const std::uint16_t /*core_id*/, const std::uint64_t /*key*/, const std::int64_t value)
{
    _server-> send(_client_id, std::to_string(value));
}

void ResponseHandler::missing(const std::uint16_t /*core_id*/, const std::uint64_t key)
{
    _server-> send(_client_id, std::to_string(key));
}




Server::Server(const std::uint64_t port,
               const std::uint16_t count_channels) noexcept
    : _port(port), _socket(-1), _client_sockets({0U}),
      _count_channels(count_channels)
{
    this->_buffer.fill('\0');

    this->_response_handlers = reinterpret_cast<ResponseHandler*>(std::malloc(sizeof(ResponseHandler) * config::max_connections()));
    for (auto client_id = 0U; client_id < config::max_connections(); ++client_id) {
        new (&this->_response_handlers[client_id]) ResponseHandler{this, client_id};
    }

    this->_request_tasks = reinterpret_cast<RequestTask*>(std::malloc( sizeof(RequestTask) * config::max_connections()));
}

Server::~Server() {
    ::free(this->_response_handlers);
    ::free(this->_request_tasks);
}

bool Server::listen(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* tree)
{
    this->_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->_socket == 0)
    {
        return false;
    }

    auto option = std::int32_t{1};
    if (setsockopt(this->_socket, SOL_SOCKET, SO_REUSEADDR, &option, socklen_t{sizeof(std::int32_t)}) < 0)
    {
        return false;
    }

    auto address = sockaddr_in{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(this->_port);

    if (bind(this->_socket, reinterpret_cast<sockaddr *>(&address), sizeof(sockaddr_in)) < 0)
    {
        return false;
    }

    if (::listen(this->_socket, 1024) < 0)
    {
        return false;
    }

    auto address_length = socklen_t{sizeof(sockaddr_in)};
    auto socket_descriptors = fd_set{};
    auto max_socket_descriptor = this->_socket;
    auto client_socket = std::int32_t{-1};

    while (this->_is_running)
    {
        FD_ZERO(&socket_descriptors); // NOLINT
        FD_SET(this->_socket, &socket_descriptors);

        for (auto &socket_descriptor : this->_client_sockets)
        {
            if (socket_descriptor > 0)
            {
                FD_SET(socket_descriptor, &socket_descriptors);
            }

            max_socket_descriptor = std::max(max_socket_descriptor, std::int32_t(socket_descriptor));
        }

        auto timeout = timeval{};
        timeout.tv_usec = 10000;
        const auto count_ready_selectors =
            select(max_socket_descriptor + 1, &socket_descriptors, nullptr, nullptr, &timeout);

        if (count_ready_selectors > 0)
        {
            if (FD_ISSET(this->_socket, &socket_descriptors))
            {
                if ((client_socket = accept(this->_socket, reinterpret_cast<sockaddr *>(&address), &address_length)) <
                    0)
                {
                    return false;
                }
                this->add_client(client_socket);
            }

            for (auto i = 0U; i < this->_client_sockets.size(); ++i)
            {
                const auto client = this->_client_sockets[i];
                if (FD_ISSET(client, &socket_descriptors))
                {
                    const auto read_bytes = read(client, this->_buffer.data(), this->_buffer.size());
                    if (read_bytes == 0U)
                    {
                        ::close(client);
                        this->_client_sockets[i] = 0U;
                    }
                    else
                    {
                        // Copy incoming data locally.
                        RequestTask::Type request_type;
                        auto message = std::string(this->_buffer.data(), read_bytes);

                        if (message[0] == 'D')
                        {
                            auto *request_task = new (&this->_request_tasks[i]) RequestTask{tree, this->_response_handlers[i]};
                            request_task->annotate(std::uint16_t(0U));
                            mx::tasking::runtime::spawn(*request_task);
                        }
                        else
                        {
                            switch(message[0])
                            {
                            case 'I': request_type = RequestTask::Type::Insert; break;
                            case 'U': request_type = RequestTask::Type::Update; break;
                            default: request_type = RequestTask::Type::Lookup;
                            }

                            auto key = 0ULL;
                            auto index = 2U; // Skip request type and comma.
                            while(message[index] >= '0' && message[index] <= '9') {
                                key = key * 10 + (message[index++] - '0');
                            }

                            auto channel_id = std::uint16_t(this->_next_worker_id.fetch_add(1U) % this->_count_channels);
                            if (request_type == RequestTask::Type::Insert || request_type == RequestTask::Type::Lookup)
                            {
                                auto value = 0LL;
                                ++index;
                                while (message[index] >= '0' && message[index] <= '9')
                                {
                                    value = value * 10 + (message[index++] - '0');
                                }

                                auto *request_task = new (&this->_request_tasks[i]) RequestTask{tree, request_type, key, value, this->_response_handlers[i]};
                                request_task->annotate(channel_id);
                                mx::tasking::runtime::spawn(*request_task);
                            }
                            else
                            {
                                auto *request_task = new (&this->_request_tasks[i]) RequestTask{tree, RequestTask::Type::Lookup, key, this->_response_handlers[i]};
                                request_task->annotate(channel_id);
                                mx::tasking::runtime::spawn(*request_task);
                            }
                            //mx::tasking::runtime::scheduler().allocate_cores(64);
                        }
                    }
                }
            }
        }
    }

    for (const auto client : this->_client_sockets)
    {
        if (client > 0)
        {
            ::close(client);
        }
    }
    ::close(this->_socket);

    return true;
}

void Server::send(const std::uint32_t client_id, std::string &&message)
{
    const auto length = std::uint64_t(message.size());
    auto response = std::string(length + sizeof(length), '\0');

    // Write header
    std::memcpy(response.data(), static_cast<const void *>(&length), sizeof(length));

    // Write data
    std::memmove(response.data() + sizeof(length), message.data(), length);

    ::send(this->_client_sockets[client_id], response.c_str(), response.length(), 0);
}

std::uint16_t Server::add_client(const std::int32_t client_socket)
{
    for (auto i = 0U; i < this->_client_sockets.size(); ++i)
    {
        if (this->_client_sockets[i] == 0U)
        {
            this->_client_sockets[i] = client_socket;
            return i;
        }
    }

    return std::numeric_limits<std::uint16_t>::max();
}

void Server::stop() noexcept
{
    this->_is_running = false;
}
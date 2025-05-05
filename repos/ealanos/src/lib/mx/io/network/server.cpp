#include "server.h"
#include <limits>
#include <mx/tasking/runtime.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace mx::io::network;

Server::Server(std::unique_ptr<MessageHandler> &&message_handler, const std::uint16_t port,
               const std::uint16_t count_channels) noexcept
    : _port(port), _socket(-1), _client_sockets({0U}), _message_handler(std::move(message_handler)),
      _count_channels(count_channels)
{
    this->_buffer.fill('\0');
}
bool Server::listen()
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

    if (::listen(this->_socket, 3) < 0)
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
                        auto message = std::string(this->_buffer.data(), read_bytes);

                        // Spawn task that processes the message.
                        auto *task = new MessageHandlerTask(*this->_message_handler.get(), i, std::move(message));
                        task->annotate(std::uint16_t(this->_next_worker_id.fetch_add(1U) % this->_count_channels));
                        tasking::runtime::spawn(*task);
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
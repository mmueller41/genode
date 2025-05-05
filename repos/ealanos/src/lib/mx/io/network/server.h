#pragma once

#include "config.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mx/memory/fixed_size_allocator.h>
#include <mx/system/cpu.h>
#include <mx/tasking/config.h>
#include <mx/tasking/scheduler.h>
#include <mx/util/core_set.h>
#include <optional>
#include <string>

namespace mx::io::network {
class MessageHandler
{
public:
    constexpr MessageHandler() noexcept = default;
    virtual ~MessageHandler() noexcept = default;

    virtual mx::tasking::TaskResult handle(std::uint16_t worker_id, std::uint32_t client_id, std::string &&message) = 0;
};

class MessageHandlerTask final : public tasking::TaskInterface
{
public:
    MessageHandlerTask(MessageHandler &message_handler, const std::uint32_t client_id, std::string &&message) noexcept
        : _message_handler(message_handler), _client_id(client_id), _message(std::move(message))
    {
    }

    ~MessageHandlerTask() noexcept override = default;

    tasking::TaskResult execute(const std::uint16_t worker_id) override
    {
        auto result = _message_handler.handle(worker_id, _client_id, std::move(_message));
        delete this;
        return result;
    }

private:
    MessageHandler &_message_handler;
    const std::uint32_t _client_id;
    std::string _message;
};

class Server
{
public:
    Server(std::unique_ptr<MessageHandler> &&message_handler, std::uint16_t port,
           std::uint16_t count_channels) noexcept;
    ~Server() noexcept = default;

    [[nodiscard]] std::uint16_t port() const noexcept { return _port; }
    void stop() noexcept;
    void send(std::uint32_t client_id, std::string &&message);
    bool listen();

    [[nodiscard]] bool is_running() const noexcept { return _is_running; }

private:
    const std::uint16_t _port;
    std::int32_t _socket;
    std::array<std::uint32_t, config::max_connections()> _client_sockets;
    std::array<char, 2048U> _buffer;
    std::unique_ptr<MessageHandler> _message_handler;

    alignas(64) bool _is_running = true;
    alignas(64) std::atomic_uint64_t _next_worker_id{0U};
    const std::uint16_t _count_channels;

    std::uint16_t add_client(std::int32_t client_socket);
};
} // namespace mx::io::network
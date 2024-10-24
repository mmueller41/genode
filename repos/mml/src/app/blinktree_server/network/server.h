#pragma once

#include "config.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mx/memory/fixed_size_allocator.h>
#include <mx/tasking/config.h>
#include <mx/tasking/scheduler.h>
#include <mx/util/core_set.h>
#include <optional>
#include <string>
#include <db/index/blinktree/b_link_tree.h>
#include <db/index/blinktree/listener.h>

namespace application::blinktree_server::network {

class Server;
class alignas(64) ResponseHandler final : public db::index::blinktree::Listener<std::uint64_t, std::int64_t>
{
public:
    ResponseHandler(Server* server, const std::uint32_t client_id) : _server(server), _client_id(client_id) { }
    ResponseHandler(ResponseHandler&&) noexcept = default;
    ~ResponseHandler() = default;

    void inserted(std::uint16_t core_id, const std::uint64_t key, const std::int64_t value) override;
    void updated(std::uint16_t core_id, const std::uint64_t key, const std::int64_t value) override;
    void removed(std::uint16_t core_id, const std::uint64_t key) override;
    void found(std::uint16_t core_id, const std::uint64_t key, const std::int64_t value) override;
    void missing(std::uint16_t core_id, const std::uint64_t key) override;

private:
    Server* _server;
    std::uint32_t _client_id;
};

class alignas(64) RequestTask final : public mx::tasking::TaskInterface
{
public:
    enum Type { Insert, Update, Lookup, Debug };

    RequestTask(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* tree, const Type type, const std::uint64_t key, ResponseHandler& response_handler) noexcept
        : _tree(tree), _type(type), _key(key), _response_handler(response_handler) { }
    RequestTask(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* tree, const Type type, const std::uint64_t key, const std::int64_t value, ResponseHandler& response_handler) noexcept
        : _tree(tree), _type(type), _key(key), _value(value), _response_handler(response_handler) { }
    RequestTask(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* tree, ResponseHandler& response_handler) noexcept
        : _tree(tree), _type(Type::Debug), _response_handler(response_handler) { }
    ~RequestTask() noexcept = default;

    mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

private:
    db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* _tree;
    Type _type;
    std::uint64_t _key;
    std::uint64_t _value;
    ResponseHandler& _response_handler;
};

class Server
{
public:
    Server(std::uint64_t port,
           std::uint16_t count_channels) noexcept;
    ~Server();

    [[nodiscard]] std::uint16_t port() const noexcept { return _port; }
    void stop() noexcept;
    void send(std::uint32_t client_id, std::string &&message);
    bool listen(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* tree);

    [[nodiscard]] bool is_running() const noexcept { return _is_running; }

private:
    const std::uint64_t _port;
    std::int32_t _socket;
    std::array<std::uint32_t, config::max_connections()> _client_sockets;
    std::array<char, 2048U> _buffer;

    ResponseHandler* _response_handlers;
    RequestTask *_request_tasks;

    alignas(64) bool _is_running = true;
    alignas(64) std::atomic_uint64_t _next_worker_id{0U};
    const std::uint16_t _count_channels;

    std::uint16_t add_client(std::int32_t client_socket);
};
} // namespace mx::io::network
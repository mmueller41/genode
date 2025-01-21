#pragma once

#include "config.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

/* B-link tree includes */
#include <db/index/blinktree/b_link_tree.h>
#include <db/index/blinktree/listener.h>

/* lwIP wrapper for Genode's NIC session */
#include <mxip/mxnic_netif.h>
#include <mxip/genode_init.h>
#include <libc/component.h>

/* Genode includes */
#include <timer_session/connection.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>

/* MxTasking includes*/
#include <mx/memory/fixed_size_allocator.h>
#include <mx/memory/dynamic_size_allocator.h>
#include <mx/tasking/config.h>
#include <mx/tasking/scheduler.h>
#include <mx/util/core_set.h>

/* lwIP includes */
namespace Lwip {
    extern "C" {
        #include <lwip/opt.h>
        #include <lwip/tcp.h>
        #include <lwip/ip_addr.h>
    }
}

namespace application::blinktree_server::network {

    class ResponseHandler;
    class RequestTask;
    class ReceiveTask;
    class Server
    {
    public:
        enum states
        {
            NONE = 0,
            ACCEPTED,
            RECEIVED,
            CLOSING,
            CLOSED
        };

        struct state
        {
            std::uint8_t state;
            std::uint8_t retries;
            struct Lwip::tcp_pcb *pcb;
            struct Lwip::pbuf *p;
            struct Lwip::pbuf *tx;
            std::uint16_t channel_id;
            std::uint64_t id;
        };
        Server(Libc::Env &env, std::uint64_t port,
               std::uint16_t count_channels, Timer::Connection &timer, Genode::Heap &alloc) noexcept;
        ~Server();

        [[nodiscard]] std::uint16_t port() const noexcept { return _port; }
        void stop() noexcept;
        void send(struct Server::state *s, std::string &&message);
        bool listen(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t> *tree);
        void parse(struct Server::state *s, std::string &message);

        [[nodiscard]] bool is_running() const noexcept { return _is_running; }
        
        static void tcp_send(struct Lwip::tcp_pcb *tpcb, struct state *s)
        {
            using namespace Lwip;
            struct Lwip::pbuf *ptr;
            Lwip::err_t rc = Lwip::ERR_OK;


            while ((rc == Lwip::ERR_OK) && (s->tx != nullptr) /* && (s->tx->len <= tcp_sndbuf(tpcb) */)
            {
                ptr = s->tx;
                // Genode::log("Sending response");
                rc = Lwip::tcp_write(tpcb, ptr->payload, ptr->len, 1);
                if (rc == Lwip::ERR_OK)
                {
                    std::uint16_t plen;

                    plen = ptr->len;

                    s->tx = ptr->next;
                    if (s->tx != nullptr)
                    {
                        Lwip::pbuf_ref(s->tx);
                    }
                    Lwip::tcp_output(tpcb);
                    Lwip::pbuf_free(ptr);
                }
                else if (rc == Lwip::ERR_MEM)
                {
                    Genode::warning("Low on memory. Defering to poll()");
                    s->tx = ptr;
                }
                else
                {
                    Genode::warning("An error ", static_cast<unsigned>(rc), " occured.");
                }
            }
        }

        static void tcpbtree_close(struct Lwip::tcp_pcb *tpcb, struct state *s)
        {
            if (s->pcb != tpcb) {
                Genode::error("Tried closing connection with invalid session state");
                return;
            }
            Lwip::tcp_arg(tpcb, NULL);
            Lwip::tcp_sent(tpcb, NULL);
            Lwip::tcp_recv(tpcb, NULL);
            Lwip::tcp_poll(tpcb, NULL, 0);
            Lwip::tcp_err(tpcb, nullptr);

            Server::tcp_free(s);

            Lwip::tcp_close(tpcb);
        }

        /* tcp_recv */
        static Lwip::err_t _handle_tcp_recv(void *arg, struct Lwip::tcp_pcb *tpcb, struct Lwip::pbuf *p, Lwip::err_t err);

        /* tcp_err */
        static void _handle_tcp_error(void *arg, Lwip::err_t err)
        {
            struct state *s;
            LWIP_UNUSED_ARG(err);

            s = static_cast<state *>(arg);

            Server::tcp_free(s);
        }

        /* tcp_poll */
        static Lwip::err_t _handle_tcp_poll(void *arg, struct Lwip::tcp_pcb *tpcb);

        /* tcp_sent */
        static Lwip::err_t _handle_tcp_sent(void *arg, struct Lwip::tcp_pcb *tpcb, std::uint16_t len);

        /* helper function for free */
        static void tcp_free(struct state *s)
        {
            // Genode::log("Freeing state obj s=", s);
            if (s)
            {
                if (s->p)
                    Lwip::pbuf_free(s->p);
                if (s->tx)
                    Lwip::pbuf_free(s->tx);
                delete s; // Lwip::mem_free(s);
            }
        }

        static Server *get_instance() { return _myself; }

        static void free_handler_task(std::uint16_t core_id, void* task)
        {
            Server::get_instance()->_handler_allocator->free(task);
        }

        static void free_task(void* task)
        {
            Server::get_instance()->_task_allocator->free(task);
        }

    private:
        static Server *_myself;
        const std::uint64_t _port;
        struct Lwip::tcp_pcb *_socket;
        Libc::Env &_env;

        std::array<struct Lwip::tcp_pcb *, config::max_connections()> _client_sockets;
        std::array<char, 2048U> _buffer;
        static ReceiveTask *_receive_tasks;

        alignas(64) bool _is_running = true;
        alignas(64) std::atomic_uint64_t _next_worker_id{0U};
        const std::uint16_t _count_channels;

        std::uint16_t add_client(Lwip::tcp_pcb *client_socket);

        /* Genode environment for NIC session */
        Genode::Attached_rom_dataspace _config;
        Genode::Heap &_alloc;
        Timer::Connection &_timer;

        /* lwIP network device (NIC session wrapper) */
        Lwip::Nic_netif _netif;
        db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t> *_tree{nullptr};

        std::unique_ptr<mx::memory::dynamic::Allocator> _handler_allocator{nullptr};
        std::unique_ptr<mx::memory::dynamic::Allocator> _task_allocator{nullptr};

        /************************************************
         *  lwIP callback API: TCP callback functions
         ************************************************/

        /* tcp_accept */
        static Lwip::err_t
        _handle_tcp_connect(void *arg, struct Lwip::tcp_pcb *newpcb, Lwip::err_t err);

       

        /* helper function for close() */

};

class alignas(64) ResponseHandler final : public db::index::blinktree::Listener<std::uint64_t, std::int64_t>
{
public:
    ResponseHandler(Server* server, Server::state *s, std::uint16_t _core_id) : _server(server), _s(s), core_id(_core_id) { }
    ResponseHandler(ResponseHandler&&) noexcept = default;
    ~ResponseHandler() = default;

    void inserted(std::uint16_t core_id, const std::uint64_t key, const std::int64_t value) override;
    void updated(std::uint16_t core_id, const std::uint64_t key, const std::int64_t value) override;
    void removed(std::uint16_t core_id, const std::uint64_t key) override;
    void found(std::uint16_t core_id, const std::uint64_t key, const std::int64_t value) override;
    void missing(std::uint16_t core_id, const std::uint64_t key) override;

private:
    Server* _server;
    Server::state *_s;
    std::uint16_t core_id{0};
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

class alignas(64) ReceiveTask final : public mx::tasking::TaskInterface
{
    public:
        ReceiveTask(Server::state *state, Lwip::pbuf *pb) : _state(state), _pbuf(pb) {}

        mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

    private:
        Server::state *_state;
        Lwip::pbuf *_pbuf;
};

class alignas(64) AcceptTask final : public mx::tasking::TaskInterface
{
    public:
        AcceptTask(Lwip::tcp_pcb *newpcb) : _pcb(newpcb) {}

        mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

    private:
        Lwip::tcp_pcb *_pcb;
};
} // namespace mx::io::network
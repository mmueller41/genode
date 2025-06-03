#pragma once

#include "config.h"
#include "ealanos/memory/hamstraaja.h"
#include "lwip/pbuf.h"
#include "mx/memory/global_heap.h"
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

namespace application::echo_server::network {

    class ResponseHandler;
    class RequestTask;
	class ReceiveTask;

	using namespace Lwip;
	
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

		struct Wakeup_scheduler : Mxip::Nic_netif::Wakeup_scheduler {
				Mxip::Nic_netif *nic{nullptr};

				void schedule_nic_server_wakeup() override { nic->wakeup_nic_server(); }

				void set_nic(Mxip::Nic_netif *nic) { this->nic = nic; }

				Wakeup_scheduler() = default;
		} _wakeup_scheduler;

		struct state
        {
            std::uint8_t state;
            std::uint8_t retries;
            struct tcp_pcb *pcb;
            struct pbuf *p;
            struct pbuf *tx;
            std::uint16_t channel_id;
            std::uint64_t id;
        };
        Server(Libc::Env &env, std::uint64_t port,
               std::uint16_t count_channels, Timer::Connection &timer, Genode::Heap &alloc, Ealan::Memory::Hamstraaja<128, 4*4096> *talloc, Mxip::Nic_netif::Payload_allocator *palloc) noexcept;
        ~Server();

        [[nodiscard]] std::uint16_t port() const noexcept { return _port; }
        void stop() noexcept;
        void send(struct Server::state *s, std::string &&message);
        bool listen();
        void parse(struct Server::state *s, std::string &message);

        [[nodiscard]] bool is_running() const noexcept { return _is_running; }
        
        static void tcp_send(struct tcp_pcb *tpcb, struct state *s)
        {
            using namespace Lwip;
            struct pbuf *ptr;
            err_t rc = ERR_OK;

			if (!s)
				return;
			
            while ((rc == ERR_OK) && (s->tx != nullptr) /* && (s->tx->len <= tcp_sndbuf(tpcb) */)
            {
                ptr = s->tx;
                // Genode::log("Sending response");
                rc = tcp_write(tpcb, ptr->payload, ptr->len, 1);
                if (rc == ERR_OK)
                {
                    std::uint16_t plen;

                    plen = ptr->len;

                    s->tx = ptr->next;
                    if (s->tx != nullptr)
                    {
                        pbuf_ref(s->tx);
                    }
                    tcp_output(tpcb);
                    pbuf_free(ptr);
                }
                else if (rc == ERR_MEM)
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

        static void tcpbtree_close(struct tcp_pcb *tpcb, struct state *s)
        {
            if (!s || s->pcb != tpcb) {
                Genode::error("Tried closing connection with invalid session state");
                return;
            }
            tcp_arg(tpcb, NULL);
            tcp_sent(tpcb, NULL);
            tcp_recv(tpcb, NULL);
            tcp_poll(tpcb, NULL, 0);
            tcp_err(tpcb, nullptr);

            Genode::log("Unregistered handlers");

            tcp_close(tpcb);
            Server::tcp_free(s);
        }

        /* tcp_recv */
        static err_t _handle_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

        /* tcp_err */
        static void _handle_tcp_error(void *arg, err_t err)
        {
            struct state *s;
            LWIP_UNUSED_ARG(err);

            s = static_cast<state *>(arg);

            Server::tcp_free(s);
        }

        /* tcp_poll */
        static err_t _handle_tcp_poll(void *arg, struct tcp_pcb *tpcb);

        /* tcp_sent */
        static err_t _handle_tcp_sent(void *arg, struct tcp_pcb *tpcb, std::uint16_t len);

        /* helper function for free */
        static void tcp_free(struct state *s)
        {
            // Genode::log("Freeing state obj s=", s);
            if (s)
            {
                if (s->p)
                    pbuf_free(s->p);
				if (s->tx) pbuf_free(s->tx);
				Genode::log("Freeing state object ", s);
				mx::memory::GlobalHeap::free(s); // mem_free(s);
				Genode::log("Freed state object");
            }
        }

        static Server *get_instance() { return _myself; }

        static void free_handler_task(std::uint16_t core_id, void* task)
        {
            mx::memory::GlobalHeap::free(task);
        }

        static void free_task(void* task)
        {
			mx::memory::GlobalHeap::free(task);
        }

    private:
        static Server *_myself;
        const std::uint64_t _port;
        struct tcp_pcb *_socket;
        Libc::Env &_env;

        std::array<char, 2048U> _buffer;
        static ReceiveTask *_receive_tasks;

        alignas(64) bool _is_running = true;
        alignas(64) std::atomic_uint64_t _next_worker_id{0U};
        const std::uint16_t _count_channels;

        std::uint16_t add_client(tcp_pcb *client_socket);

        /* Genode environment for NIC session */
        Genode::Attached_rom_dataspace _config;
        Genode::Heap &_alloc;
        Timer::Connection &_timer;

        /* lwIP network device (NIC session wrapper) */
        Mxip::Nic_netif _netif;

        /************************************************
         *  lwIP callback API: TCP callback functions
         ************************************************/

        /* tcp_accept */
        static err_t
        _handle_tcp_connect(void *arg, struct tcp_pcb *newpcb, err_t err);

       

        /* helper function for close() */

};

class alignas(64) ReceiveTask final : public mx::tasking::TaskInterface
{
    public:
        ReceiveTask(Server::state *state, pbuf *pb, std::size_t len) : _state(state), _payload(pb), _length(len) {}

        mx::tasking::TaskResult execute(std::uint16_t core_id, std::uint16_t channel_id) override;

    private:
        Server::state *_state;
		pbuf          *_payload;
		std::size_t    _length;
};

} // namespace mx::io::network
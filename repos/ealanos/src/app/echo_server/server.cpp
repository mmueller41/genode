#include "server.h"
#include "base/log.h"
#include "ealanos/memory/hamstraaja.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/tcpbase.h"
#include "mx/memory/global_heap.h"
#include "mx/tasking/task.h"
#include "mxip_lock.h"
#include "mxnic_netif.h"
#include <cstring>
#include <limits>
#include <mx/tasking/runtime.h>
#include <unistd.h>
#include <db/index/blinktree/lookup_task.h>
#include <db/index/blinktree/insert_value_task.h>
#include <db/index/blinktree/update_task.h>
#include <mx/system/topology.h>
#include <tukija/syscall-generic.h>
#include <tukija/syscalls.h>
#include <iostream>

using namespace application::echo_server::network;

Server *Server::_myself;

ReceiveTask *Server::_receive_tasks = nullptr;

Server::Server(Libc::Env &env,
               const std::uint64_t port,
               const std::uint16_t count_channels, Timer::Connection &timer, Genode::Heap &alloc, Ealan::Memory::Hamstraaja<128, 4*4096> *talloc, Mxip::Nic_netif::Payload_allocator *palloc) noexcept
    : _port(port), _socket(nullptr), 
      _count_channels(count_channels), _env{env}, _config(env, "config"), _alloc(alloc),  _timer(timer), _netif(env, _alloc, _config.xml(), _wakeup_scheduler, talloc, palloc)
{
    Server::_myself = this;
    this->_buffer.fill('\0');

	_wakeup_scheduler.set_nic(&_netif);
	
    _receive_tasks = static_cast<ReceiveTask*>(mx::memory::GlobalHeap::allocate_cache_line_aligned(65536 * sizeof(ReceiveTask)));

}

Server::~Server() {
}

bool Server::listen()
{
    _socket = tcp_new();

    if (!_socket) {
        Genode::error("Failed to create server socket");
        return false;
    }

    err_t rc = tcp_bind(_socket, &ip_addr_any, _port);
    if (rc != ERR_OK) {
        Genode::error("Failed to bind server socket to port ", _port);
        return false;
    }

    _socket = tcp_listen_with_backlog(_socket, 64);
    tcp_accept(_socket, &Server::_handle_tcp_connect);

    return true;
}

class Send_task : public mx::tasking::TaskInterface
{
    private:
        struct Server::state *_s;
        std::string _message;
    
    public:
        Send_task(Server::state *s, std::string message) : _s(s), _message(message) {}

        mx::tasking::TaskResult execute(std::uint16_t, std::uint16_t) override
		{
            using namespace Lwip;

            if (_s->state == Server::CLOSED || _s->state == Server::CLOSING)  {
                Genode::warning("Tried to send over socket that is to be closed");
                //Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_remove();
            }

            pbuf *ptr = pbuf_alloc(PBUF_TRANSPORT, _message.length(), PBUF_RAM);

            if (!(_s->pcb) || !_s) {
				Genode::error("Tried sending over invalid pcb");
                //Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_remove();
            }

                ptr->payload = static_cast<void *>(const_cast<char *>(_message.c_str()));
            ptr->len = _message.length();

            if (ptr->len > tcp_sndbuf(_s->pcb)) {
				Genode::warning("Not enough space in send buffer");
				//Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_remove();
			}

            err_t rc = ERR_OK;
			{
                rc = tcp_write(_s->pcb, ptr->payload, ptr->len, TCP_WRITE_FLAG_COPY);
            }
            if (rc == ERR_OK)
            {
				tcp_output(_s->pcb);
                pbuf_free(ptr);
			} else {
				if (_s->tx == nullptr)
                    _s->tx = ptr;
                else {
                    pbuf_cat(_s->tx, ptr);
                }
            }
            //Server::free_task(static_cast<void *>(this));
            return mx::tasking::TaskResult::make_remove();
        }
};

void
Server::send(Server::state *s, std::string &&message)
{
    //GENODE_LOG_TSC(1);
    const auto length = std::uint64_t(message.size());
    auto response = std::string(length + sizeof(length), '\0');

    // Write header
    std::memcpy(response.data(), static_cast<const void *>(&length), sizeof(length));

    // Write data
    std::memmove(response.data() + sizeof(length), message.data(), length);

    auto task = mx::tasking::runtime::new_task<Send_task>(0, s, response);//new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(Send_task))) Send_task(s, response);
    task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
    mx::tasking::runtime::spawn(*task);
}

void Server::stop() noexcept
{
	this->_is_running = false;
}

class Close_task : public mx::tasking::TaskInterface
{
    private:
        Server::state &_s;

    public:
        Close_task(Server::state &s) : _s(s) {}

        mx::tasking::TaskResult execute(std::uint16_t, std::uint16_t)
        {
            Genode::log("Closing connection for ", static_cast<void *>(_s.pcb) , " and state object ", static_cast<void*>(&_s));
            Server::tcpbtree_close(_s.pcb, &_s);
			_s.state = Server::CLOSED;
            //Server::free_task(static_cast<void *>(this));
			Genode::log("Closed connection");
		
            return mx::tasking::TaskResult::make_remove();
        }
};

/***********
 * LWIP callback function definitions
 ***********/
err_t Server::_handle_tcp_connect(void *arg, struct tcp_pcb *newpcb, err_t err)
{

    struct state *s;

    static uint64_t count_connections = 0;

    LWIP_UNUSED_ARG(arg);

	if ((err != ERR_OK) || (newpcb == NULL)) { return ERR_VAL; }

    s = new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(struct state)))
        state(); // static_cast<struct state *>(mem_malloc(sizeof(struct state)));

    if (!s) {
        Genode::error("Failed to allocate state object for new connection.");
        return ERR_MEM;
    }
   //Genode::log("New connection #", count_connections, ": arg=", arg, " pcb=", newpcb, " s=", s, " &s=", static_cast<void*>(&s));

    s->state = states::ACCEPTED;
    s->pcb = newpcb;
    s->retries = 0;
    s->p = nullptr;
    s->tx = nullptr;
    s->channel_id = 0; //count_connections % Server::get_instance()->_count_channels;

    tcp_backlog_accepted(newpcb);
    /* Register callback functions */
    tcp_arg(newpcb, s);
    tcp_recv(newpcb, &Server::_handle_tcp_recv);
    tcp_err(newpcb, &Server::_handle_tcp_error);
    tcp_poll(newpcb, &Server::_handle_tcp_poll, 50);
    tcp_sent(newpcb, &Server::_handle_tcp_sent);
    newpcb->flags |= TF_NODELAY;

    return ERR_OK;
}

err_t Server::_handle_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) 
{
	static std::uint16_t next_receive_task = 0;
    struct state *s;
    err_t rc = ERR_OK;

    std::uint16_t next_channel_id = 0;

    s = static_cast<struct state*>(arg);

	if (!s) {
		pbuf_free(p);
		return ERR_ARG;
	}

	if (err != ERR_OK) {
		pbuf_free(p);
        return err;
    }

    if (p == nullptr) {
        s->state = CLOSING;
        auto task = mx::tasking::runtime::new_task<Close_task>(0, *s);
		//auto task = new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(Close_task))) Close_task(*s);
        if (!task) {
            Genode::warning("Failed to allocate close task");
            return ERR_MEM;
        }
        task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
        mx::tasking::runtime::spawn(*task);
		return ERR_OK;
	} else if (err != ERR_OK) {
		rc = err;
	} else if (s->state == states::ACCEPTED) {
        //s->state = states::RECEIVED;
        rc = ERR_OK;
		{
			ReceiveTask *task = nullptr;
			{
				task = mx::tasking::runtime::new_task<ReceiveTask>(0, s, p, p->len);
            }
			//ReceiveTask *task = new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(ReceiveTask))) ReceiveTask(s, payload, p->len);
            if (!task) {
                Genode::warning("Could not allocate request handler task");
                return ERR_MEM;
            }
            task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
			mx::tasking::runtime::spawn(*task);
		}
		{
			//GENODE_LOG_TSC_NAMED(1, "tcp_recved");
			tcp_recved(s->pcb, p->len);
        }
	    //pbuf_free(p);

        //Server::get_instance()->send(s, "Nope");
	} else if (s->state == states::RECEIVED) {
		//void *payload = mx::memory::GlobalHeap::allocate_cache_line_aligned(p->len);
		//std::memcpy(payload, p->payload, p->len);

		auto task = mx::tasking::runtime::new_task<ReceiveTask>(0, s, p, p->len);
        //ReceiveTask *task = new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(ReceiveTask))) ReceiveTask(s, payload, p->len);
        if (!task) {
            Genode::warning("Could not allocate request handler task");
            return ERR_MEM;
        }
        task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
        mx::tasking::runtime::spawn(*task);

		tcp_recved(s->pcb, p->len);
		//pbuf_free(p);
		
        rc = ERR_OK;
    }
    else
    {
        pbuf_free(p);
        tcp_recved(tpcb, p->tot_len);
        rc = ERR_OK;
	}


    return rc;
}

err_t Server::_handle_tcp_poll(void *arg, struct tcp_pcb *tpcb) 
{
    err_t rc;
    struct state *s;

    s = static_cast<struct state *>(arg);

    if (s) {
        if (s->tx) {
            rc = tcp_write(tpcb, s->tx->payload, s->tx->len, 1);
            if (rc == ERR_OK) {
                tcp_output(tpcb);
                pbuf *ptr = s->tx;
                if (ptr->next) {
                    s->tx = ptr->next;
                    pbuf_ref(s->tx);
                }
                tcp_recved(tpcb, ptr->len);
                pbuf_free(ptr);
            }
            // TODO: process remaning pbuf entry
        } else {
            /*if (s->state == states::CLOSING) {
                Server::tcpbtree_close(tpcb, s);
            }*/
        }
        rc = ERR_OK;
    } else {
        tcp_abort(tpcb);
        rc = ERR_ABRT;
    } 

    return ERR_OK;
}


err_t Server::_handle_tcp_sent(void *arg, struct tcp_pcb *tpcb, std::uint16_t len)
{
    //GENODE_LOG_TSC(1);
	struct state *s = static_cast<struct state *>(arg);

	if (!s)
		return ERR_ARG;
	
    s->retries = 0;

    if (s->tx) {
        err_t rc = tcp_write(tpcb, s->tx->payload, s->tx->len, 1);
        if (rc == ERR_OK) {
            tcp_output(tpcb);
            pbuf *ptr = s->tx;
            if (ptr->next) {
                s->tx = ptr->next;
                pbuf_ref(s->tx);
            }
            tcp_recved(tpcb, ptr->len);
            pbuf_free(ptr);
        }
        tcp_sent(tpcb, &Server::_handle_tcp_sent); // Genode::log("In _handle_tcp_sent");
    }

    return ERR_OK;
}

mx::tasking::TaskResult application::echo_server::network::ReceiveTask::execute(std::uint16_t core_id, std::uint16_t channel_id)
{
    err_t rc = ERR_OK;

	//GENODE_LOG_TSC_NAMED(1, "ReceiveTask");
	if (!_payload || !_payload->payload)
		return mx::tasking::TaskResult::make_remove();

    std::string request = std::string(static_cast<char*>(_payload->payload), _length);
    auto key = 0ULL;
    auto index = 2U; // Skip request type and comma.
    while (request[index] >= '0' && request[index] <= '9')
    {
        key = key * 10 + (request[index++] - '0');
    }
    Server::get_instance()->send(_state, std::to_string(key));

	pbuf_free(_payload);
	//mx::memory::GlobalHeap::free(_payload);
	
    //Server::free_task(static_cast<void *>(this));
    return mx::tasking::TaskResult::make_remove();
}
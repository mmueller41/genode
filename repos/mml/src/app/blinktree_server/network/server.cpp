#include "server.h"
#include <limits>
#include <mx/tasking/runtime.h>
#include <unistd.h>
#include <db/index/blinktree/lookup_task.h>
#include <db/index/blinktree/insert_value_task.h>
#include <db/index/blinktree/update_task.h>
#include <mx/system/topology.h>
#include <nova/syscall-generic.h>
#include <nova/syscalls.h>
#include <iostream>

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

    return mx::tasking::TaskResult::make_succeed_and_remove(request_task);
}

void ResponseHandler::inserted(const std::uint16_t /*core*/, const std::uint64_t key, const std::int64_t /*value*/)
{
    _server->send(_s, std::to_string(key));
    Server::free_handler_task(core_id, static_cast<void *>(this));
}

void ResponseHandler::updated(const std::uint16_t /*core_id*/, const std::uint64_t key, const std::int64_t /*value*/)
{
    _server-> send(_s, std::to_string(key));
    Server::free_handler_task(core_id, static_cast<void *>(this));
}

void ResponseHandler::removed(const std::uint16_t /*core_id*/, const std::uint64_t key)
{
    _server-> send(_s, std::to_string(key));
    Server::free_handler_task(core_id, static_cast<void *>(this));
}

void ResponseHandler::found(const std::uint16_t /*core_id*/, const std::uint64_t /*key*/, const std::int64_t value)
{
    _server-> send(_s, std::to_string(value));
    Server::free_handler_task(core_id, static_cast<void *>(this));
}

void ResponseHandler::missing(const std::uint16_t /*core_id*/, const std::uint64_t key)
{
    _server-> send(_s, std::to_string(key));
    Server::free_handler_task(core_id, static_cast<void *>(this));
}

Server *Server::_myself;

ReceiveTask *Server::_receive_tasks = nullptr;

Server::Server(Libc::Env &env,
               const std::uint64_t port,
               const std::uint16_t count_channels, Timer::Connection &timer, Genode::Heap &alloc) noexcept
    : _port(port), _socket(nullptr), _client_sockets({nullptr}),
      _count_channels(count_channels), _env{env}, _config(env, "config"), _alloc(alloc),  _timer(timer), _netif(env, _alloc, _config.xml())
{
    Server::_myself = this;
    this->_buffer.fill('\0');

    _receive_tasks = static_cast<ReceiveTask*>(mx::memory::GlobalHeap::allocate_cache_line_aligned(65536 * sizeof(ReceiveTask)));

    _handler_allocator.reset(new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(mx::memory::dynamic::Allocator))) mx::memory::dynamic::Allocator());

    _task_allocator.reset(new (mx::memory::GlobalHeap::allocate_cache_line_aligned(sizeof(mx::memory::dynamic::Allocator))) mx::memory::dynamic::Allocator());
}

Server::~Server() {
}

bool Server::listen(db::index::blinktree::BLinkTree<std::uint64_t, std::int64_t>* tree)
{
    _socket = Lwip::tcp_new();

    if (!_socket) {
        Genode::error("Failed to create server socket");
        return false;
    }

    Lwip::err_t rc = Lwip::tcp_bind(_socket, &Lwip::ip_addr_any, _port);
    if (rc != Lwip::ERR_OK) {
        Genode::error("Failed to bind server socket to port ", _port);
        return false;
    }

    _socket = Lwip::tcp_listen_with_backlog(_socket, 64);
    Lwip::tcp_accept(_socket, &Server::_handle_tcp_connect);

    this->_tree = tree;

    return true;
}

void Server::parse(struct Server::state *s, std::string &message)
{
    RequestTask::Type request_type;

    std::uint64_t i = s->id;

    if (message[0] == 'D')
    {
        auto response_handler = new (_handler_allocator->allocate(0, 64, sizeof(ResponseHandler))) ResponseHandler(this, s, 0);
        //auto *request_task = new (&this->_request_tasks[i]) RequestTask{this->_tree, *response_handler};
        auto *request_task = mx::tasking::runtime::new_task<RequestTask>(0, this->_tree, *response_handler);
        request_task->annotate(std::uint16_t(0U));
        mx::tasking::runtime::spawn(*request_task);
    }
    else
    {
        switch (message[0])
        {
        case 'I':
            request_type = RequestTask::Type::Insert;
            break;
        case 'U':
            request_type = RequestTask::Type::Update;
            break;
        default:
            request_type = RequestTask::Type::Lookup;
        }

        auto key = 0ULL;
        auto index = 2U; // Skip request type and comma.
        while (message[index] >= '0' && message[index] <= '9')
        {
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
            
            auto response_handler = new (_handler_allocator->allocate(mx::system::topology::node_id(channel_id), 64, sizeof(ResponseHandler))) ResponseHandler(this, s, channel_id);
            auto *request_task = mx::tasking::runtime::new_task<RequestTask>(channel_id, this->_tree, request_type, key, value, *response_handler);
            request_task->annotate(channel_id);
            mx::tasking::runtime::spawn(*request_task);
        }
        else
        {
            //auto *request_task = new (&this->_request_tasks[i]) RequestTask{this->_tree, RequestTask::Type::Lookup, key, this->_response_handlers[i]};
            auto response_handler = new (_handler_allocator->allocate(mx::system::topology::node_id(channel_id), 64, sizeof(ResponseHandler))) ResponseHandler(this, s, channel_id);
            auto *request_task = mx::tasking::runtime::new_task<RequestTask>(channel_id, this->_tree, request_type, key, *response_handler);
            request_task->annotate(channel_id);
            mx::tasking::runtime::spawn(*request_task);
        }
        mx::tasking::runtime::scheduler().allocate_cores(64);
    }
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
            Lwip::pbuf *ptr = nullptr;

            if (_s->state == Server::CLOSED || _s->state == Server::CLOSING) {
                Genode::warning("Tried to send over socket that is to be closed");
                Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_null();
            }

            ptr = Lwip::pbuf_alloc(Lwip::PBUF_TRANSPORT, _message.length(), Lwip::PBUF_RAM);

            if (!(_s->pcb) || !_s) {
                Genode::error("Tried sending over invalid pcb");
                Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_null();
            }

            if (!ptr)
            {
                Genode::error("No memory for sending packet.");
                Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_null();
            }

            if (ptr >= reinterpret_cast<void *>(0x7FFF80000000UL) || _s->pcb >= reinterpret_cast<void *>(0x7FFF80000000UL))
            {
                Genode::error("Allocated buffer or pcb is at non-canonical address. Aborting. ptr=", static_cast<void*>(ptr), " pcb=", static_cast<void*>(_s->pcb), " s=", static_cast<void*>(_s));
                Server::free_task(static_cast<void *>(this));
                return mx::tasking::TaskResult::make_null();
            }

                ptr->payload = static_cast<void *>(const_cast<char *>(_message.c_str()));
            ptr->len = _message.length();

            if (ptr->len > tcp_sndbuf(_s->pcb))
                Genode::warning("Not enough space in send buffer");

            Lwip::err_t rc = Lwip::ERR_OK;
            {
                rc = Lwip::tcp_write(_s->pcb, ptr->payload, ptr->len, TCP_WRITE_FLAG_COPY);
            }
            if (rc == Lwip::ERR_OK)
            {
                Lwip::tcp_output(_s->pcb);
                Lwip::pbuf_free(ptr);
            } else {
                if (_s->tx == nullptr)
                    _s->tx = ptr;
                else {
                    Lwip::pbuf_cat(_s->tx, ptr);
                }
            }
            Server::free_task(static_cast<void *>(this));
            return mx::tasking::TaskResult::make_null();
        }
};

void
Server::send(struct state *s, std::string &&message)
{
     
    const auto length = std::uint64_t(message.size());
    auto response = std::string(length + sizeof(length), '\0');

    // Write header
    std::memcpy(response.data(), static_cast<const void *>(&length), sizeof(length));

    // Write data
    std::memmove(response.data() + sizeof(length), message.data(), length);

    auto task = new (Server::get_instance()->_task_allocator->allocate(0, 64, sizeof(Send_task))) Send_task(s, response);
    task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
    mx::tasking::runtime::spawn(*task);
}

std::uint16_t Server::add_client(Lwip::tcp_pcb* client_socket)
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
            Server::free_task(static_cast<void *>(this));
            return mx::tasking::TaskResult::make_null();
        }
};

/***********
 * LWIP callback function definitions
 ***********/
Lwip::err_t Server::_handle_tcp_connect(void *arg, struct Lwip::tcp_pcb *newpcb, Lwip::err_t err)
{

    struct state *s;

    static uint64_t count_connections = 0;

    LWIP_UNUSED_ARG(arg);

    if ((err != Lwip::ERR_OK) || (newpcb == NULL)) {
        return Lwip::ERR_VAL;
    }

    //Genode::log("Incoming request");

    s = new (Lwip::mem_malloc(sizeof(struct state))) state(); // static_cast<struct state *>(Lwip::mem_malloc(sizeof(struct state)));

    if (!s) {
        Genode::error("Failed to allocate state object for new connection.");
        return Lwip::ERR_MEM;
    }
   //Genode::log("New connection #", count_connections, ": arg=", arg, " pcb=", newpcb, " s=", s, " &s=", static_cast<void*>(&s));

    s->state = states::ACCEPTED;
    s->pcb = newpcb;
    s->retries = 0;
    s->p = nullptr;
    s->tx = nullptr;
    s->channel_id = 0; //count_connections % Server::get_instance()->_count_channels;

    Lwip::tcp_backlog_accepted(newpcb);
    /* Register callback functions */
    Lwip::tcp_arg(newpcb, s);
    Lwip::tcp_recv(newpcb, &Server::_handle_tcp_recv);
    Lwip::tcp_err(newpcb, &Server::_handle_tcp_error);
    Lwip::tcp_poll(newpcb, &Server::_handle_tcp_poll, 50);
    Lwip::tcp_sent(newpcb, &Server::_handle_tcp_sent);
    newpcb->flags |= TF_NODELAY;

    return Lwip::ERR_OK;
}

Lwip::err_t Server::_handle_tcp_recv(void *arg, struct Lwip::tcp_pcb *tpcb, struct Lwip::pbuf *p, Lwip::err_t err) 
{
    static std::uint16_t next_receive_task = 0;
    struct state *s;
    Lwip::err_t rc = Lwip::ERR_OK;

    std::uint16_t next_channel_id = 0;

    s = static_cast<struct state*>(arg);

    if (err != Lwip::ERR_OK) {
        return err;
    }

    if (p == nullptr) {
        s->state = CLOSING;
        auto task = new (Server::get_instance()->_task_allocator->allocate(0, 64, sizeof(Close_task))) Close_task(*s);
        if (!task) {
            Genode::warning("Failed to allocate close task");
            return Lwip::ERR_MEM;
        }
        task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
        mx::tasking::runtime::spawn(*task);
        Lwip::pbuf_free(p);
        rc = Lwip::ERR_OK;
    } else if (err != Lwip::ERR_OK) {
        rc = err;
    } else if (s->state == states::ACCEPTED) {
        s->state == states::RECEIVED;

        // TODO: parse message and spawn request task here
        rc = Lwip::ERR_OK;
        {
            ReceiveTask *task = new (Server::get_instance()->_task_allocator->allocate(0, 64, sizeof(ReceiveTask))) ReceiveTask(s, p);
            if (!task) {
                Genode::warning("Could not allocate request handler task");
                return Lwip::ERR_MEM;
            }
            task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
            mx::tasking::runtime::spawn(*task);
        }
        Lwip::tcp_recved(s->pcb, p->len);
        //Server::get_instance()->send(s, "Nope");
    }
    else if (s->state == states::RECEIVED)
    {
        ReceiveTask *task = new (Server::get_instance()->_task_allocator->allocate(0, 64, sizeof(ReceiveTask))) ReceiveTask(s, p);
        if (!task) {
            Genode::warning("Could not allocate request handler task");
            return Lwip::ERR_MEM;
        }
        task->annotate(static_cast<mx::tasking::TaskInterface::channel>(s->channel_id));
        mx::tasking::runtime::spawn(*task);
        Lwip::tcp_recved(s->pcb, p->len);
        //Server::get_instance()->send(s, "Nope");

        rc = Lwip::ERR_OK;
    }
    else
    {
        Lwip::tcp_recved(tpcb, p->tot_len);
        Lwip::pbuf_free(p);
        rc = Lwip::ERR_OK;
    }

    return rc;
}

Lwip::err_t Server::_handle_tcp_poll(void *arg, struct Lwip::tcp_pcb *tpcb) 
{
    Lwip::err_t rc;
    struct state *s;

    //GENODE_LOG_TSC(1);
    s = static_cast<struct state *>(arg);

    if (s) {
        if (s->tx) {
            rc = Lwip::tcp_write(tpcb, s->tx->payload, s->tx->len, 1);
            if (rc == Lwip::ERR_OK) {
                Lwip::tcp_output(tpcb);
                Lwip::pbuf *ptr = s->tx;
                if (ptr->next) {
                    s->tx = ptr->next;
                    Lwip::pbuf_ref(s->tx);
                }
                Lwip::tcp_recved(tpcb, ptr->len);
                Lwip::pbuf_free(ptr);
            }
            // TODO: process remaning pbuf entry
        } else {
            /*if (s->state == states::CLOSING) {
                Server::tcpbtree_close(tpcb, s);
            }*/
        }
        rc = Lwip::ERR_OK;
    } else {
        Lwip::tcp_abort(tpcb);
        rc = Lwip::ERR_ABRT;
    } 

    return Lwip::ERR_OK;
}


Lwip::err_t Server::_handle_tcp_sent(void *arg, struct Lwip::tcp_pcb *tpcb, std::uint16_t len)
{
    //GENODE_LOG_TSC(1);
    struct state *s = static_cast<struct state *>(arg);
    s->retries = 0;

    if (s->tx) {
        Lwip::err_t rc = Lwip::tcp_write(tpcb, s->tx->payload, s->tx->len, 1);
        if (rc == Lwip::ERR_OK) {
            Lwip::tcp_output(tpcb);
            Lwip::pbuf *ptr = s->tx;
            if (ptr->next) {
                s->tx = ptr->next;
                Lwip::pbuf_ref(s->tx);
            }
            Lwip::tcp_recved(tpcb, ptr->len);
            Lwip::pbuf_free(ptr);
        }
        tcp_sent(tpcb, &Server::_handle_tcp_sent); // Genode::log("In _handle_tcp_sent");
    }

    return Lwip::ERR_OK;
}

mx::tasking::TaskResult application::blinktree_server::network::ReceiveTask::execute(std::uint16_t core_id, std::uint16_t channel_id)
{
    Lwip::err_t rc = Lwip::ERR_OK;

    /*rc = Lwip::tcp_write(_state->pcb, _pbuf->payload, _pbuf->len, 3);
     Lwip::tcp_output(_state->pcb);
     if (rc == Lwip::ERR_OK) {
         Lwip::tcp_recved(_state->pcb, _pbuf->tot_len);
         Lwip::pbuf_free(_pbuf);
     } else if (rc == Lwip::ERR_MEM) {
         Genode::warning("Out of memory");
     }*/

    //Genode::log("Executing application task");
    //Server::get_instance()->send(_state, "Nope");
    // Server::tcp_send(_state->pcb, _state);

    std::string request = std::string(static_cast<char*>(_pbuf->payload), _pbuf->len);
    Server::get_instance()->parse(_state, request);

    Lwip::pbuf_free(_pbuf);

    Server::free_task(static_cast<void *>(this));
    return mx::tasking::TaskResult::make_null();
}
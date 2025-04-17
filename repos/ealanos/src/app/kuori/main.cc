#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <base/log.h>
#include <lwip/lwip_genode_init.h>
#include <lwip/nic_netif.h>
#include <timer_session/connection.h>
#include <ealanos/laucher/connection.h>

#include <cstdio>

namespace Lwip {
    extern "C" {
        #include <lwip/arch.h>
        #include <lwip/opt.h>
        #include <lwip/tcp.h>
        #include <lwip/dns.h>
        #include <lwip/ip_addr.h>
    }
}

namespace Ealan::Kuori {
    using namespace Genode;
    class Main;
}

class Ealan::Kuori::Main
{
    private:
        Env &_env;
        Heap &_heap;

        Ealan::Launcher_connection &_hoitaja;

        struct Wakeup_scheduler : Lwip::Nic_netif::Wakeup_scheduler
        {
            Lwip::Nic_netif *nic{nullptr};

            void schedule_nic_server_wakeup() override
            {
                nic->wakeup_nic_server();
            }

            void set_nic(Lwip::Nic_netif *nic)
            {
                this->nic = nic;
            }

            Wakeup_scheduler() = default;
        } _wakeup_scheduler;

        enum states
        {
            NONE = 0,
            ACCEPTED,
            RECEIVED,
            CLOSING
        };

        struct state
        {
            uint8_t state;
            uint8_t retries;
            struct Lwip::tcp_pcb *pcb;
            struct Lwip::pbuf *p;
        };
        
        Lwip::tcp_pcb *listen_socket;
        uint16_t _port{8080};

        struct Tcp_connection
        {
            static Main *kuori;
            /**
             * @brief Free state and lwip objects
             *
             * @param s
             */
            static void free(struct state *s)
            {
                if (s != nullptr) {
                    if (s->p) {
                        Lwip::pbuf_free(s->p);
                    }
                    Lwip::mem_free(s);
                }
            }

            static void close(struct Lwip::tcp_pcb *tpcb, struct state *s)
            {
                Lwip::tcp_arg(tpcb, nullptr);
                Lwip::tcp_sent(tpcb, nullptr);
                Lwip::tcp_recv(tpcb, nullptr);
                Lwip::tcp_err(tpcb, nullptr);
                Lwip::tcp_poll(tpcb, nullptr, 0);

                free(s);
                Lwip::tcp_close(tpcb);
            }

            static void send(struct Lwip::tcp_pcb *tpcb, struct state *s)
            {
                struct Lwip::pbuf *ptr;
                Lwip::err_t wr_err = Lwip::ERR_OK;

                while ((wr_err == Lwip::ERR_OK) && (s->p != nullptr))
                {
                    ptr = s->p;

                    wr_err = Lwip::tcp_write(tpcb, ptr->payload, ptr->len, 3);
                    if (wr_err == Lwip::ERR_OK) {
                        uint16_t plen;

                        plen = ptr->len;
                        s->p = ptr->next;

                        if (s->p) {
                            Lwip::pbuf_ref(s->p);
                        }
                        Lwip::pbuf_free(ptr);
                        Lwip::tcp_recved(tpcb, plen);
                    } else if (wr_err == Lwip::ERR_MEM) {
                        s->p = ptr;
                    } else {
                        Genode::error("Encountered LwIP error code ", static_cast<signed int>(wr_err));
                    }
                }
            }

            static void error(void *arg, Lwip::err_t error)
            {
                struct state *s;

                LWIP_UNUSED_ARG(error);

                s = static_cast<struct state *>(arg);

                free(s);
            }

            static Lwip::err_t poll(void *arg, struct Lwip::tcp_pcb *tpcb)
            {
                Lwip::err_t rc;
                struct state *s;

                s = static_cast<struct state *>(arg);
                if (s) {
                    if (s->p) {
                        send(tpcb, s);
                    } else {
                        if (s->state == CLOSING) {
                            close(tpcb, s);
                        }
                    }
                    rc = Lwip::ERR_OK;
                } else {
                    Lwip::tcp_abort(tpcb);
                    rc = Lwip::ERR_ABRT;
                }
                return rc;
            }

            static Lwip::err_t sent(void *arg, struct Lwip::tcp_pcb *tpcb, [[maybe_unused]] Lwip::u16_t len)
            {
                struct state *s;

                s = static_cast<struct state *>(arg);
                s->retries = 0;

                if (s->p) {
                    Lwip::tcp_sent(tpcb, sent);
                    send(tpcb, s);
                } else {
                    if (s->state == CLOSING) {
                        close(tpcb, s);
                    }
                }
                return Lwip::ERR_OK;
            }

            static Lwip::err_t receive(void *arg, struct Lwip::tcp_pcb *tpcb, struct Lwip::pbuf *p, Lwip::err_t err)
            {
                struct state *s;
                Lwip::err_t rc = Lwip::ERR_OK;

                s = static_cast<struct state *>(arg);
                
                if (!p) {
                    s->state = CLOSING;
                    if (!s->p) {
                        close(tpcb, s);
                    } else {
                        send(tpcb, s);
                    }
                    rc = Lwip::ERR_OK;
                } else if (err != Lwip::ERR_OK) {
                    rc = err;
                } else if (s->state == ACCEPTED) {
                    s->state = RECEIVED;
                    kuori->parse_and_launch(tpcb, p, s);
                }
                else if (s->state == RECEIVED)
                {
                    if (!s->p) {
                        s->p = p;
                        kuori->parse_and_launch(tpcb, p, s);
                    }
                    else
                    {
                        struct Lwip::pbuf *ptr;
                        ptr = s->p;
                        Lwip::pbuf_cat(ptr, p);
                    }
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

            static Lwip::err_t accept([[maybe_unused]] void *arg, struct Lwip::tcp_pcb *newpcb, Lwip::err_t err)
            {
                Lwip::err_t rc;
                struct state *s;

                LWIP_UNUSED_ARG(arg);

                if ((err != Lwip::ERR_OK) || (newpcb == nullptr)) {
                    return Lwip::ERR_VAL;
                }

                s = static_cast<struct state *>(Lwip::mem_malloc(sizeof(struct state)));
                if (s) {
                    s->state = ACCEPTED;
                    s->pcb = newpcb;
                    s->retries = 0;
                    s->p = nullptr;

                    Lwip::tcp_arg(newpcb, s);
                    Lwip::tcp_recv(newpcb, receive);
                    Lwip::tcp_err(newpcb, error);
                    Lwip::tcp_poll(newpcb, poll, 0);
                    Lwip::tcp_sent(newpcb, sent);

                    rc = Lwip::ERR_OK;

                    Genode::log("Accepted new connection from ", const_cast<const char*>(Lwip::ipaddr_ntoa(&newpcb->remote_ip)));
                }
                else
                {
                    rc = Lwip::ERR_MEM;
                }
                return rc;
            }

        };
        
        void init()
        {
            listen_socket = Lwip::tcp_new_ip_type(Lwip::IPADDR_TYPE_ANY);

            if (!listen_socket) {
                Genode::error("Failed to create server socket.");
            }

            Lwip::err_t err;
            err = Lwip::tcp_bind(listen_socket, &Lwip::ip_addr_any, _port);
            
            if (err == Lwip::ERR_OK) {
                listen_socket = Lwip::tcp_listen_with_backlog(listen_socket, 255);
                Lwip::tcp_accept(listen_socket, Tcp_connection::accept);
            } else {
                Genode::error("Failed to bind socket.");
            }

            Tcp_connection::kuori = this;

            Genode::log("Kuori v0.1 - Remote shell for EalánOS");
        }

        Lwip::Nic_netif _netif;

    public:
        Main(Env &env, Genode::Heap &heap, Xml_node config, Ealan::Launcher_connection &hoitaja) : _env(env), _heap(heap), _hoitaja(hoitaja), _wakeup_scheduler(), _netif(env, _heap, config, _wakeup_scheduler) {

            _wakeup_scheduler.set_nic(&_netif);
            init();
        }

        void parse_and_launch(Lwip::tcp_pcb *tpcb, Lwip::pbuf *p, state *state)
        {
            try {
                Genode::log("Parsing input of length ", p->len);
                Genode::log(static_cast<const char *>(p->payload));
                Xml_node start_node(static_cast<char *>(p->payload));
                //Genode::log(start_node);

                Genode::String<90> name = start_node.attribute_value("name", Genode::String<90>());
                char msg[120];

                std::sprintf(msg, "Lauching %s\n", name.string());

                Genode::String<640> cell_cfg(static_cast<const char*>(p->payload));
                Genode::log(cell_cfg);
                _hoitaja.launch(cell_cfg);

                Lwip::pbuf *response_buffer = Lwip::pbuf_alloc(Lwip::PBUF_TRANSPORT, Genode::strlen(msg), Lwip::PBUF_RAM);

                Lwip::pbuf_take(response_buffer, msg, Genode::strlen(msg));

                state->p = response_buffer;
                Tcp_connection::send(tpcb, state);
            }
            catch (Genode::Xml_attribute::Invalid_syntax)
            {
                Genode::error("Invalid config supplied.");
                Genode::String<100> response("Invalid cell configuration.\n");

                Lwip::pbuf *response_buffer = Lwip::pbuf_alloc(Lwip::PBUF_TRANSPORT, response.length(), Lwip::PBUF_RAM);
                Lwip::pbuf_take(response_buffer, response.string(), response.length());

                state->p = response_buffer;
                Tcp_connection::send(tpcb, state);
            }
        }
};

Ealan::Kuori::Main *Ealan::Kuori::Main::Tcp_connection::kuori;

void Libc::Component::construct(Libc::Env &env)
{
    Genode::Attached_rom_dataspace _config{env, "config"};
    Genode::Xml_node config = _config.xml();

    static Genode::Heap heap{env.ram(), env.rm()};
    static Timer::Connection timer{env};

    Lwip::genode_init(heap, timer);
    
    Genode::Thread *myself = Genode::Thread::myself();
    Genode::Affinity::Location loc = myself->affinity();
    Genode::Affinity affinity(env.cpu().affinity_space(), loc);

    static Ealan::Launcher_connection hoitaja{env, affinity};

    static Ealan::Kuori::Main kuori(env, heap, config, hoitaja);
}
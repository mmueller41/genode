#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <base/log.h>
#include <lwip/lwip_genode_init.h>
#include <lwip/nic_netif.h>
#include <timer_session/connection.h>

namespace Lwip {
    extern "C" {
        #include <lwip/tcp.h>
        #include <lwip/dns.h>
        #include <lwip/ip_addr.h>
        #include "tcpecho_raw.h"
    }
}

class Main 
{
    private:
        Genode::Env &_env;

        static Lwip::err_t _handle_tcp_accept(void *arg, struct Lwip::tcp_pcb *newpcb, Lwip::err_t err) {
            Genode::log("New client connection: arg=", arg, " newpcb=", newpcb);
            return err;
        }
        
        struct Wakeup_scheduler : Lwip::Nic_netif::Wakeup_scheduler
        {

            /**
             * Lwip::Nic_netif::Wakeup_scheduler interface
             *
             * Called from Lwip::Nic_netif.
             */
            void schedule_nic_server_wakeup() override
            {
                _nic->wakeup_nic_server();
            }

            void set_nic(Lwip::Nic_netif *nic)
            {
                _nic = nic;
            }

            Wakeup_scheduler()
            {
            }

            Lwip::Nic_netif *_nic{nullptr};

        } _wakeup_scheduler;
        Lwip::Nic_netif _netif;

    public:
        Main(Genode::Env &env, Genode::Allocator &alloc, Genode::Xml_node config) : _env(env), _wakeup_scheduler(), _netif(env, alloc, config, _wakeup_scheduler) {
            _wakeup_scheduler.set_nic(&_netif);
            Lwip::tcpecho_raw_init();
            /*struct Lwip::tcp_pcb *socket = Lwip::tcp_new();
            Genode::log("Server socket = ", socket);
            if (!socket)
            {
                Genode::error("Failed to create new TCP server socket");
                _env.parent().exit(-1);
            }

            Lwip::err_t rc = Lwip::tcp_bind(socket, &Lwip::ip_addr_any, 8080);
            if (rc != Lwip::ERR_OK) {
                Genode::error("Failed to bind socket");
                _env.parent().exit(-1);
            }

            socket = Lwip::tcp_listen(socket);
            Genode::log("Server socket after listen() = ", socket);

            Lwip::tcp_accept(socket, &_handle_tcp_accept);*/
        }
};

void Libc::Component::construct(Libc::Env &env) {
    Genode::Attached_rom_dataspace _config{env, "config"};
    Genode::Xml_node config = _config.xml();
    Genode::log(config);
    static Genode::Heap _alloc{env.ram(), env.rm()};
    static Timer::Connection _timer(env);

    Lwip::genode_init(_alloc, _timer);
    static Main main(env, _alloc, config);
}
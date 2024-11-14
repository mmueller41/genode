#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <libc/component.h>
#include <base/heap.h>
#include <base/mutex.h>

/* CiAO/IP includes */
#include <ciao-ip/hw/hal/NetworkDevice.h>
#include <ciao-ip/ipstack/api/Setup.h>
#include <ciao-ip/ipstack/router/Router.h>
#include <ciao-ip/ipstack/ipv4/IPv4.h>

class Raw_nic : public hw::hal::NetworkDevice
{
    private:
        Nic::Packet_allocator _nic_tx_alloc;
        Nic::Connection _nic;
        Genode::Mutex _mutex{};

        unsigned char _mac[6];

        Genode::Io_signal_handler<Raw_nic> _link_state_handler;
		Genode::Io_signal_handler<Raw_nic> _rx_packet_handler;
		Genode::Io_signal_handler<Raw_nic> _tx_ready_handler;

        enum
        {
            PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
            BUFF_SIZE = Nic::Session::QUEUE_SIZE * PACKET_SIZE
        };

    public:
        void handle_link_state()
        {
            Genode::log("Link state requested.");
        }

        void handle_rx_packets()
        {
            auto &rx = *_nic.rx();

            while (rx.packet_avail() && rx.ready_to_ack() ) {
                Nic::Packet_descriptor packet = rx.get_packet();
                Genode::log("Received packet of size ", packet.size());

                void *eth_frame = rx.packet_content(packet);

                demux(eth_frame, packet.size());
                if (!rx.ready_to_ack())
                    return;

                rx.acknowledge_packet(packet);
            }
        }

        void handle_tx_ready()
        {
            auto &tx = *_nic.tx();

            while (tx.ack_avail())
                tx.release_packet(tx.get_acked_packet());

            Genode::log("Send packets");
        }

        void send(const void* pkt, unsigned pkt_size ) override
        {
            try {
                Nic::Packet_descriptor pkt_desc = _nic.tx()->alloc_packet(pkt_size);
                void *pkt_base = _nic.tx()->packet_content(pkt_desc);

                Genode::memcpy(pkt_base, pkt, pkt_size);

                _nic.tx()->submit_packet(pkt_desc);
            } catch (Nic::Packet_stream_source<Nic::Session::Policy>::Packet_alloc_failed) {
                Genode::warning("Packet allocation has failed.");
            }
        }

        Raw_nic(Genode::Env &env, Genode::Allocator &alloc) : _nic_tx_alloc(&alloc), _nic(env, &_nic_tx_alloc, BUFF_SIZE, BUFF_SIZE), _link_state_handler(env.ep(), *this, &Raw_nic::handle_link_state), _rx_packet_handler(env.ep(), *this, &Raw_nic::handle_rx_packets), _tx_ready_handler(env.ep(), *this, &Raw_nic::handle_tx_ready) {
            Genode::log("Created NIC session.");
            Genode::log("Registering callbacks");
            _nic.link_state_sigh(_link_state_handler);
            _nic.rx_channel()->sigh_packet_avail(_rx_packet_handler);
			_nic.rx_channel()->sigh_ready_to_ack(_rx_packet_handler);
			_nic.tx_channel()->sigh_ready_to_submit(_tx_ready_handler);
			_nic.tx_channel()->sigh_ack_avail      (_tx_ready_handler);
            Genode::log("Callbacks registered. Waiting for incoming packets...");
            Genode::log("MAC address from NIC session: ", _nic.mac_address());
            _nic.mac_address().copy(_mac);
            Genode::log("Mac address read: ", _mac[0], ":", _mac[1], ":", _mac[2], ":", _mac[3], ":", _mac[4], ":", _mac[5]);
            init();
            Genode::log("Initialized CiAO/IP");

            ipstack::Interface *interface = IP::getInterface(0);
            if (interface) {
                interface->setIPv4Addr(10, 0, 3,55);
                interface->setIPv4Subnetmask(255, 255, 255, 0);
                interface->setIPv4Up(true);
                ipstack::Router::Inst().ipv4_set_gateway_addr(ipstack::IPv4_Packet::convert_ipv4_addr(10, 0, 3, 1));
            }
        }

        /* CiAO/IP NetworkDevice public interface */
        const char* getName() override {
            return "eth0";
        }

        unsigned getMTU() override {
            return 1500;
        }

        const unsigned char *getAddress() override {
            return _mac;
        }

        unsigned char getType() override { return 1; }


};

void Libc::Component::construct(Libc::Env &env)
{
    static Genode::Heap heap{env.ram(), env.rm()};

    static Raw_nic _raw_nic(env, heap);

}
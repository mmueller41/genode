#include "mxnic_netif.h"
#include <base/log.h>

void Lwip::Nic_netif::handle_rx_packets()
{

    Nic::Session::Rx::Sink *rx = _nic.rx();

    bool progress = false;

    while (rx->packet_avail() && rx->ready_to_ack()) {

        try
        {
            Nic::Packet_descriptor packet = rx->try_get_packet();
            progress = true;

            Nic_netif_pbuf *nic_pbuf = new (this->_pbuf_alloc) Nic_netif_pbuf(*this, packet);

            if (!nic_pbuf) {
                Genode::warning("Could not allocate pbuf ");
                return;
            }
            pbuf* p = pbuf_alloced_custom(
                PBUF_RAW,
                packet.size(),
                PBUF_REF,
                &nic_pbuf->p,
                rx->packet_content(packet),
                packet.size());
            LINK_STATS_INC(link.recv);

            if (!p) {
                Genode::warning("Initialization of pbuf failed.");
                return;
            }

            Lwip::Receive_task *task = new (_handler_allocator->allocate(0, 64, sizeof(Lwip::Receive_task))) Lwip::Receive_task(p, _netif, *this, nic_pbuf);
            if (task == nullptr)
            {
                Genode::warning("Could not allocate task object.");
                return;
            }
            task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
            mx::tasking::runtime::spawn(*task);

            if (progress)
                wake_up_nic_server();
        }
        catch (Genode::Exception)
        {
            Genode::warning("Got signal without actual packet in queue");
        }
    }
}

void Lwip::Nic_netif::handle_tx_ready()
{
    Lwip::Tx_ready_task *task = new (_handler_allocator->allocate(0, 64, sizeof(Lwip::Tx_ready_task))) Lwip::Tx_ready_task(_nic, *this);
    if (task == nullptr)
    {
        Genode::warning("Could not allocate tx_ready task object.");
        return;
    }
    task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
    mx::tasking::runtime::spawn(*task);

    
}

void Lwip::Nic_netif::handle_link_state()
{
    Lwip::Link_state_task *task = new (_handler_allocator->allocate(0, 64, sizeof(Lwip::Link_state_task))) Lwip::Link_state_task(_nic, _netif, *this, _dhcp); // mx::tasking::runtime::new_task<Lwip::Link_state_task>(0, _nic, _netif, _dhcp);
    if (task == nullptr) {
        Genode::warning("Could not allocate link state task object.");
        return;
    }
    task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
    mx::tasking::runtime::spawn(*task);
}
#include "lwip/pbuf.h"
#include <cstring>
#include <mxip/mxnic_netif.h>
#include <mx/memory/global_heap.h>
#include <mx/tasking/runtime.h>
#include <base/log.h>

void Mxip::Nic_netif::handle_rx_packets()
{

Nic::Session::Rx::Sink *rx = _nic.rx();

bool progress = false;

while (rx->packet_avail() && rx->ready_to_ack()) {

    try
    {
        Nic::Packet_descriptor packet = rx->try_get_packet();
        progress = true;

        void *payload = _packet_alloc->alloc(sizeof(packet.size()));
        memcpy(payload, rx->packet_content(packet), packet.size());

        Mxip::Receive_task *task = new (_task_alloc->alloc(sizeof(Mxip::Receive_task))) Mxip::Receive_task(payload, packet.size(), _netif, *this);
        if (task == nullptr)
        {
            Genode::warning("Could not allocate task object.");
            return;
        }
        task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
        mx::tasking::runtime::spawn(*task);

        if (progress) {

            while (!_nic.rx()->ready_to_ack()) { _ep.wait_and_dispatch_one_io_signal(); }

            rx->try_ack_packet(packet);
            _wakeup_scheduler.schedule_nic_server_wakeup();
        }
    }
    catch (Genode::Exception)
    {
        Genode::warning("Got signal without actual packet in queue");
    }
}
}

void Mxip::Nic_netif::handle_tx_ready()
{
/*Mxip::Tx_ready_task *task = new (_task_alloc->alloc(sizeof(Mxip::Tx_ready_task)))
Mxip::Tx_ready_task(_nic, *this); if (task == nullptr)
{
    Genode::warning("Could not allocate tx_ready task object.");
    return;
}
task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
mx::tasking::runtime::spawn(*task);*/


    auto &tx       = *_nic.tx();
    bool  progress = false;

    while (tx.ack_avail()) {
        tx.release_packet(tx.try_get_acked_packet());
        _tx_saturated = false;
        progress = true;
    }

    status_callback();
    
    if (progress) _wakeup_scheduler.schedule_nic_server_wakeup();

    //return mx::tasking::TaskResult::make_remove();
}

void Mxip::Nic_netif::handle_link_state()
{
Mxip::Link_state_task *task = new (_task_alloc->alloc(sizeof(Mxip::Link_state_task))) Mxip::Link_state_task(_nic, _netif, *this, _dhcp);
if (task == nullptr) {
    Genode::warning("Could not allocate link state task object.");
    return;
}
task->annotate(static_cast<mx::tasking::TaskInterface::channel>(0));
mx::tasking::runtime::spawn(*task);
}
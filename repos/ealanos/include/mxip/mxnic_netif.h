/*
 * \brief  LwIP netif for the Nic session
 * \author Emery Hemingway
 * \date   2016-09-28
 *
 * If you want to use the lwIP API in a native Genode
 * component then this is the Nic client to use.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* TODO: Bugs
 * tcp_out.c:1410 0x1033d4c - GPF
 * tcp_in.c:1102 0x102c084 - GPF
 * tcp_out.c:1335 - GPF
 * OUT_OF_CAPS
 * PAGE_FAULT and GPFs on task->execute
 * GPF on task->next()
*/

#ifndef __LWIP__NIC_NETIF_H__
#define __LWIP__NIC_NETIF_H__

#include "base/allocator.h"
#include "mx/memory/global_heap.h"
#include "mx/system/environment.h"
#include "os/packet_stream.h"
#include "trace/timestamp.h"
#include <memory>
#if ETH_PAD_SIZE
#error ETH_PAD_SIZE defined but unsupported by lwip/nic_netif.h
#endif

#ifndef __cplusplus
#error lwip/nic_netif.h is a C++ only header
#endif

/* Genode includes */
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <base/log.h>

/* MxTasking includes */
#include <mx/tasking/runtime.h>
#include <mx/tasking/task.h>

#include "mxip_lock.h"
/* EalanOS includes */
#include <ealanos/memory/hamstraaja.h>

namespace Lwip {

extern "C" {
/* LwIP includes */
#include <lwip/netif.h>
#include <netif/etharp.h>
#if LWIP_IPV6
#include <lwip/ethip6.h>
#endif
#include <lwip/init.h>
#include <lwip/dhcp.h>
#include <lwip/dns.h>
}
} // namespace Lwip

namespace Mxip
{
	class Receive_task;
	class Tx_ready_task;
	class Link_state_task;
	class Finished_rx_task;
	class Nic_netif;

	using namespace Lwip;
} 

namespace Lwip {

	extern "C" {

		static void nic_netif_pbuf_free(pbuf *p);
		static err_t nic_netif_init(struct netif *netif);
		static err_t nic_netif_linkoutput(struct netif *netif, struct pbuf *p);
		static void  nic_netif_status_callback(struct netif *netif);
	}

	/**
	 * Metadata for packet backed pbufs
	 */
	struct Nic_netif_pbuf
	{
		struct pbuf_custom p { };
		Mxip::Nic_netif &netif;
		void              *payload;
		Genode::Packet_descriptor packet;

		Nic_netif_pbuf(Mxip::Nic_netif &nic, void *pl, Genode::Packet_descriptor pkt)
		: netif(nic), payload(pl), packet(pkt)
		{
			p.custom_free_function = nic_netif_pbuf_free;
		}
	};

	struct Rx_task_stats {
		Genode::Trace::Timestamp waiting_time;
		Genode::Trace::Timestamp execution_time;	
	};
}

class Mxip::Nic_netif
{
	public:

		friend class Mxip::Receive_task;
		friend class Mxip::Tx_ready_task;
		friend class Mxip::Link_state_task;

		struct Wakeup_scheduler : Genode::Noncopyable, Genode::Interface
		{
			virtual void schedule_nic_server_wakeup() = 0;
		};

		enum { TASK_SIZE = 128, SB_SIZE = 2 * 2048 };
		
	private:

		Genode::Entrypoint _ep;

		Wakeup_scheduler &_wakeup_scheduler;

		bool _tx_saturated = false;

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = 2048*PACKET_SIZE,
		};

		Genode::Tslab<Lwip::Nic_netif_pbuf, 32768*sizeof(Lwip::Nic_netif_pbuf)> _pbuf_alloc;

	public:
		using Payload_allocator = Ealan::Memory::Hamstraaja<PACKET_SIZE, BUF_SIZE>;

	private:	
		Ealan::Memory::Hamstraaja<128, 4 * 4096> *_task_alloc{nullptr};
		Payload_allocator *_packet_alloc{nullptr};
		Nic::Packet_allocator _nic_tx_alloc;
		Nic::Connection _nic;

		struct Lwip::netif _netif { };

		Lwip::ip_addr_t ip { };
		Lwip::ip_addr_t nm { };
		Lwip::ip_addr_t gw { };

		Genode::Io_signal_handler<Nic_netif> _link_state_handler;
		Genode::Io_signal_handler<Nic_netif> _rx_packet_handler;
		Genode::Io_signal_handler<Nic_netif> _tx_ready_handler;

		bool _dhcp { false };

		Lwip::Rx_task_stats _rx_stats[10000100];
	public:

		void free_pbuf(Lwip::Nic_netif_pbuf &pbuf)
		{
			bool msg_once = true;
			while (!_nic.rx()->ready_to_ack()) {
				//if (msg_once)
					//Genode::error("Nic rx acknowledgement queue congested, blocking to  free pbuf");
				msg_once = false;
				_ep.wait_and_dispatch_one_io_signal();
			}

			_nic.rx()->try_ack_packet(pbuf.packet);
			_wakeup_scheduler.schedule_nic_server_wakeup();
			//_packet_alloc->free(pbuf.payload);
			_packet_alloc->free(&pbuf);//destroy(_pbuf_alloc, &pbuf);
		}

		/*************************
		 ** Nic signal handlers **
		 *************************/

		void handle_link_state();
		void handle_rx_packets();

		/**
		 * Handle tx ack_avail and ready_to_submit signals
		 */
		void handle_tx_ready();	

		void configure(Genode::Xml_node const &config)
		{
			_dhcp = config.attribute_value("dhcp", false);

			using Str = Genode::String<IPADDR_STRLEN_MAX>;
			Str ip_str = config.attribute_value("ip_addr", Str());

			Genode::log("Static IP: ", ip_str);

			if (_dhcp && ip_str != "") {
				_dhcp = false;
				Lwip::netif_set_down(&_netif);
				Genode::error("refusing to configure lwIP interface with both DHCP and a static IPv4 address");
				return;
			}

			Genode::log("Bringing up NIC");
			Lwip::netif_set_up(&_netif);
			Genode::log("Set NIC up");
			
			if (ip_str != "") {
				Lwip::ip_addr_t ipaddr;
				if (!Lwip::ipaddr_aton(ip_str.string(), &ipaddr)) {
					Genode::error("lwIP configured with invalid IP address '",ip_str,"'");
					throw ip_str;
				}

				Genode::log("Setting IP for NIC");
				netif_set_ipaddr(&_netif, ip_2_ip4(&ipaddr));

				Genode::log("Set IP address for NIC");
				if (config.has_attribute("netmask")) {
					Str str = config.attribute_value("netmask", Str());
					Lwip::ip_addr_t ip;
					Lwip::ipaddr_aton(str.string(), &ip);
					Lwip::netif_set_netmask(&_netif, ip_2_ip4(&ip));
				}

				if (config.has_attribute("gateway")) {
					Str str = config.attribute_value("gateway", Str());
					Lwip::ip_addr_t ip;
					Lwip::ipaddr_aton(str.string(), &ip);
					Lwip::netif_set_gw(&_netif, ip_2_ip4(&ip));
				}

			}

			if (config.has_attribute("nameserver")) {
				/*
				 * LwIP does not use DNS internally, but the application
				 * should expect "dns_getserver" to work regardless of
				 * how the netif configures itself.
				 */
				Str str = config.attribute_value("nameserver", Str());
				Lwip::ip_addr_t ip;
				Lwip::ipaddr_aton(str.string(), &ip);
				//dns_setserver(0, &ip);
			}

			Genode::log("Finished configuring NIC");
			handle_link_state();
		}

		Nic_netif(Genode::Env &env, Genode::Allocator &alloc, Genode::Xml_node config,
		          Wakeup_scheduler                         &wakeup_scheduler,
		          Ealan::Memory::Hamstraaja<128, 4 * 4096> *task_alloc, Payload_allocator *palloc)
			: _ep(env, 4*4096, "mxip_ep", env.cpu().affinity_space().location_of_index(0)), _wakeup_scheduler(wakeup_scheduler), _pbuf_alloc(alloc),
			  _task_alloc(task_alloc),
			  _packet_alloc(palloc),
			  _nic_tx_alloc(&alloc),
			_nic(env, &_nic_tx_alloc,
			     BUF_SIZE, BUF_SIZE,
			     config.attribute_value("label", Genode::String<160>("mxip")).string()),
			_link_state_handler(_ep, *this, &Nic_netif::handle_link_state),
			_rx_packet_handler( _ep, *this, &Nic_netif::handle_rx_packets),
			_tx_ready_handler(_ep, *this, &Nic_netif::handle_tx_ready)
		{
			Genode::memset(&_netif, 0x00, sizeof(_netif));

			{
				Lwip::ip4_addr_t v4dummy;
				IP4_ADDR(&v4dummy, 0, 0, 0, 0);

				netif* r = Lwip::netif_add(&_netif, &v4dummy, &v4dummy, &v4dummy,
				                     this, nic_netif_init, ethernet_input);
				if (r == NULL) {
					Genode::error("failed to initialize Nic to lwIP interface");
					throw r;
				}
			}

			Lwip::netif_set_default(&_netif);
			Lwip::netif_set_status_callback(
				&_netif, nic_netif_status_callback);
			Lwip::nic_netif_status_callback(&_netif);

			configure(config);
		}

		virtual ~Nic_netif() { }

		Lwip::netif& lwip_netif() { return _netif; }

		bool tx_saturated() const { return _tx_saturated; }

		/**
		* Status callback to override in subclass
		 */
		virtual void status_callback() { }

		/**
		 * Callback issued by lwIP to initialize netif struct
		 *
		 * \noapi
		 */
		err_t init()
		{
			/*
			 * XXX: hostname and MTU could probably be
			 * set in the Nic client constructor
			 */

#if LWIP_NETIF_HOSTNAME
			/* Initialize interface hostname */
			_netif.hostname = "";
#endif /* LWIP_NETIF_HOSTNAME */

			_netif.name[0] = 'e';
			_netif.name[1] = 'n';

			_netif.output = etharp_output;
#if LWIP_IPV6
			_netif.output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

			_netif.linkoutput      = nic_netif_linkoutput;

			/* Set physical MAC address */
			Nic::Mac_address const mac = _nic.mac_address();
			for(int i=0; i<6; ++i)
				_netif.hwaddr[i] = mac.addr[i];

			_netif.mtu        = 1500; /* XXX: just a guess */
			_netif.hwaddr_len = ETHARP_HWADDR_LEN;
			_netif.flags      = NETIF_FLAG_BROADCAST |
			                    NETIF_FLAG_ETHARP    |
			                    NETIF_FLAG_LINK_UP;

			/* set Nic session signal handlers */
			_nic.link_state_sigh(_link_state_handler);
			_nic.rx_channel()->sigh_packet_avail(_rx_packet_handler);
			_nic.rx_channel()->sigh_ready_to_ack(_rx_packet_handler);
			_nic.tx_channel()->sigh_ready_to_submit(_tx_ready_handler);
			_nic.tx_channel()->sigh_ack_avail      (_tx_ready_handler);

			return ERR_OK;
		}

		/**
		 * Callback issued by lwIP to write a Nic packet
		 *
		 * \noapi
		 */
		err_t linkoutput(struct pbuf *p)
		{
			auto &tx = *_nic.tx();

			/* flush acknowledgements */
			while (tx.ack_avail()) {
				tx.release_packet(tx.try_get_acked_packet());
				_tx_saturated = false;
			}

			if (!tx.ready_to_submit()) {
				Genode::error("lwIP: Nic packet queue congested, cannot send packet");
				_tx_saturated = true;
				return ERR_WOULDBLOCK;
			}

			Nic::Packet_descriptor packet;
			try { packet = tx.alloc_packet(p->tot_len); }
			catch (...) {
				Genode::error("lwIP: Nic packet allocation failed, cannot send packet");
				_tx_saturated = true;
				return ERR_WOULDBLOCK;
			}

			/*
			 * We iterate over the pbuf chain until we have read the entire
			 * pbuf into the packet.
			 */
			char *dst = tx.packet_content(packet);
			for (struct pbuf *q = p; q != nullptr; q = q->next) {
				char const *src = (char const *)q->payload;
				Genode::memcpy(dst, src, q->len);
				dst += q->len;
			}

			tx.try_submit_packet(packet);
			_wakeup_scheduler.schedule_nic_server_wakeup();
			LINK_STATS_INC(link.xmit);
			return ERR_OK;
		}

		bool ready()
		{
			return netif_is_up(&_netif) &&
				!ip_addr_isany(&_netif.ip_addr);
		}

		/**
		 * Trigger submission of deferred packet-stream signals
		 */
		void wakeup_nic_server()
		{
			_nic.rx()->wakeup();
			_nic.tx()->wakeup();
		}

		void print_stats()
		{
			for (int id = 0; id < 1000000; id++) {
				Genode::log("rx: id =", id, " wait=", _rx_stats[id].waiting_time, " etime: ", _rx_stats[id].execution_time);
			}
		}

};

/**************************
 ** MxIP handler tasks   **
 **************************/

class Mxip::Receive_task : public mx::tasking::TaskInterface
{
	public:

		Receive_task(void *payload, size_t len, struct Lwip::netif &netif, Mxip::Nic_netif &net, Genode::Trace::Timestamp arrival, unsigned long id, Genode::Packet_descriptor packet)
			: _netif(netif), _payload(payload), _length(len), _net(net), _arrival(arrival), _id(id), _packet(packet)
		{
		}

		mx::tasking::TaskResult execute(std::uint16_t, std::uint16_t) override
		{
			Genode::Trace::Timestamp execution = Genode::Trace::timestamp();
			Lwip::Nic_netif_pbuf *nic_pbuf = new (_net._packet_alloc->alloc(sizeof(Lwip::Nic_netif_pbuf)))
					Lwip::Nic_netif_pbuf(_net, _payload, _packet);

			if (!nic_pbuf) {
				Genode::warning("Could not allocate pbuf ");
				_net._task_alloc->free(this);
				return mx::tasking::TaskResult::make_null();
			}

			pbuf *p = pbuf_alloced_custom(PBUF_RAW, _length, PBUF_REF, &nic_pbuf->p, _payload,
			                              _length);
			LINK_STATS_INC(link.recv);

			if (!p) {
				Genode::warning("Initialization of pbuf failed.");
				_net._task_alloc->free(this);
				return mx::tasking::TaskResult::make_null();
			}

			err_t rc = _netif.input(p, &_netif);
			
			if (rc != ERR_OK) {
				Genode::error("error forwarding Nic packet to lwIP: error=",
				              static_cast<std::int16_t>(rc));
				pbuf_free(p);
			}
			Genode::Trace::Timestamp processed = Genode::Trace::timestamp();

			_net._rx_stats[_id].waiting_time =
				((execution - _arrival) * 1000) / mx::system::Environment::get_cpu_freq();
			_net._rx_stats[_id].execution_time = ((processed - execution) * 1000) / mx::system::Environment::get_cpu_freq();
			_net._task_alloc->free(this);
			return mx::tasking::TaskResult::make_null();
			//return mx::tasking::TaskResult::make_remove();
		}

	private:

		struct Lwip::netif         &_netif;
		void                       *_payload;
		std::size_t 				_length;
		Mxip::Nic_netif            &_net;
		Genode::Trace::Timestamp    _arrival;
		unsigned long               _id;
		Genode::Packet_descriptor   _packet;
};

class Mxip::Tx_ready_task : public mx::tasking::TaskInterface
{
	public:

		Tx_ready_task(Nic::Connection &nic, Mxip::Nic_netif &netif) : _nic(nic), _netif(netif) { }
		mx::tasking::TaskResult execute(std::uint16_t, std::uint16_t) override
		{
			//GENODE_LOG_TSC(1);
			auto &tx       = *_nic.tx();
			bool  progress = false;

			while (tx.ack_avail()) {
				tx.release_packet(tx.try_get_acked_packet());
				//_netif._tx_saturated = false;
				progress = true;
			}

			_netif.status_callback();
			
			if (progress) _netif._wakeup_scheduler.schedule_nic_server_wakeup();

			_netif._task_alloc->free(this);
			return mx::tasking::TaskResult::make_null();
			//return mx::tasking::TaskResult::make_remove();
		}

	private:

		Nic::Connection &_nic;
		Mxip::Nic_netif &_netif;
};

class Mxip::Link_state_task : public mx::tasking::TaskInterface
{
	public:

		Link_state_task(Nic::Connection &nic, Lwip::netif &netif, Mxip::Nic_netif &nic_netif,
		                bool dhcp)
			: _nic(nic), _nic_netif(nic_netif), _netif(netif), _dhcp(dhcp)
		{
		}

		mx::tasking::TaskResult execute(std::uint16_t, std::uint16_t) override
		{
			/*
			 * if the application wants to be informed of the
			 * link state then it should use 'set_link_callback'
			 */

			//GENODE_LOG_TSC_NAMED(1, "Link state task");
			if (_nic.link_state()) {
				Lwip::netif_set_link_up(&_netif);
				if (_dhcp) {
				    err_t err = dhcp_start(&_netif);
				    if (err != ERR_OK) {
				        Genode::error("failed to configure lwIP interface with DHCP, error ", -err);
				    }
				} else {
				    Lwip::dhcp_inform(&_netif);
				}
			} else {
				Lwip::netif_set_link_down(&_netif);
				if (_dhcp) {
					Lwip::dhcp_release_and_stop(&_netif);
				}
			}
			//_nic_netif._task_alloc->free(this);
			//return mx::tasking::TaskResult::make_null();
			return mx::tasking::TaskResult::make_remove();
		}

	private:

		Nic::Connection &_nic;
		Mxip::Nic_netif &_nic_netif;
		Lwip::netif     &_netif;
		bool             _dhcp;
};

/**************************
 ** LwIP netif callbacks **
 **************************/

namespace Lwip {
	extern "C" {

/**
 * Free a packet buffer backed pbuf
 */
static void nic_netif_pbuf_free(pbuf *p)
{
	Nic_netif_pbuf *nic_pbuf = reinterpret_cast<Nic_netif_pbuf *>(p);
	nic_pbuf->netif.free_pbuf(*nic_pbuf);
}


/**
 * Initialize the netif
 */
static err_t nic_netif_init(struct netif *netif)
{
	Mxip::Nic_netif *nic_netif = (Mxip::Nic_netif *)netif->state;
	return nic_netif->init();
}


/**
 * Send a raw packet to the Nic session
 */
static err_t nic_netif_linkoutput(struct netif *netif, struct pbuf *p)
{
	Mxip::Nic_netif *nic_netif = (Mxip::Nic_netif *)netif->state;
	return nic_netif->linkoutput(p);
}


static void nic_netif_status_callback(struct netif *netif)
{
	Mxip::Nic_netif *nic_netif = (Mxip::Nic_netif *)netif->state;

	if (netif_is_up(netif)) {
		if (!ip4_addr_isany(netif_ip4_addr(netif))) {
			using Str = Genode::String<IPADDR_STRLEN_MAX>;
			Str address((char const*)ip4addr_ntoa(netif_ip4_addr(netif)));
			Str netmask((char const*)ip4addr_ntoa(netif_ip4_netmask(netif)));
			Str gateway((char const*)ip4addr_ntoa(netif_ip4_gw(netif)));

			Genode::log("MxIP Nic interface up"
				" address=", address,
				" netmask=", netmask,
				" gateway=", gateway);
		}
	} else {
			Genode::log("MxIP Nic interface down");
	}

	//nic_netif->status_callback();
}

	}
}

#endif /* __LWIP__NIC_NETIF_H__ */

/*
 * \brief  Configuration file for LwIP, adapt it to your needs.
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2009-11-10
 *
 * See lwip/src/include/lwip/opt.h for all options
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__LWIPOPTS_H__
#define __LWIP__LWIPOPTS_H__

/* Genode includes */
#include <base/fixed_stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Use lwIP without OS-awareness
 */
#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 0

#define LWIP_DNS                    0  /* DNS support */
#define LWIP_DHCP                   0  /* DHCP support */
#define LWIP_SOCKET                 0  /* LwIP socket API */
#define LWIP_NETIF_LOOPBACK         0  /* Looping back to same address? */
#define LWIP_STATS                  0  /* disable stating */
#define LWIP_ICMP                   0
#define LWIP_SNMP                   0
#define LWIP_TCP_TIMESTAMPS         0
#define TCP_LISTEN_BACKLOG              255
#define TCP_MSS                         1460
#define TCP_WND                     (46 * TCP_MSS)
#define TCP_SND_BUF                 (46 * TCP_MSS)
#define LWIP_WND_SCALE                  3
#define TCP_RCV_SCALE                   2
#define TCP_SND_QUEUELEN                ((512 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))

#define LWIP_NETIF_STATUS_CALLBACK  1  /* callback function used for interface changes */
#define LWIP_NETIF_LINK_CALLBACK    1  /* callback function used for link-state changes */
#define   LWIP_SUPPORT_CUSTOM_PBUF  1

#define LWIP_SINGLE_NETIF           1

#define TCP_QUEUE_OOSEQ 1
#define LWIP_PCB_ARRAY 1
/***********************************
 ** Checksum calculation settings **
 ***********************************/

/* checksum calculation for outgoing packets can be disabled if the hardware supports it */
#define LWIP_CHECKSUM_ON_COPY       1  /* calculate checksum during memcpy */

/*********************
 ** Memory settings **
 *********************/

#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1
#define MEMP_MEM_INIT               0
#define MEMP_NUM_TCP_SEG            (2*TCP_SND_QUEUELEN)
/* MEM_ALIGNMENT > 4 e.g. for x86_64 are not supported, see Genode issue #817 */
#define MEM_ALIGNMENT               4

#define DEFAULT_ACCEPTMBOX_SIZE   128
#define TCPIP_MBOX_SIZE           128

#define RECV_BUFSIZE_DEFAULT        (512*1024)

#define PBUF_POOL_SIZE             8192

#define MEMP_NUM_SYS_TIMEOUT        64
#define MEMP_NUM_TCP_PCB           512
#define MEMP_NUM_PBUF              (128*4096)

#ifndef MEMCPY
#define MEMCPY(dst,src,len)             genode_memcpy(dst,src,len)
#endif

#ifndef MEMMOVE
#define MEMMOVE(dst,src,len)            genode_memmove(dst,src,len)
#endif

/********************
 ** Debug settings **
 ********************/
#define LWIP_NOASSERT            1

/* #define LWIP_DEBUG */
/* #define DHCP_DEBUG      LWIP_DBG_ON */
/* #define ETHARP_DEBUG    LWIP_DBG_ON */
/* #define NETIF_DEBUG     LWIP_DBG_ON */
/* #define PBUF_DEBUG      LWIP_DBG_ON */
/* #define API_LIB_DEBUG   LWIP_DBG_ON */
/* #define API_MSG_DEBUG   LWIP_DBG_ON */
/* #define SOCKETS_DEBUG   LWIP_DBG_ON */
/* #define ICMP_DEBUG      LWIP_DBG_ON */
/* #define INET_DEBUG      LWIP_DBG_ON */
/* #define IP_DEBUG        LWIP_DBG_ON */
/* #define IP_REASS_DEBUG  LWIP_DBG_ON */
/* #define RAW_DEBUG       LWIP_DBG_ON */
/* #define MEM_DEBUG       LWIP_DBG_ON */
/* #define MEMP_DEBUG      LWIP_DBG_ON */
/* #define SYS_DEBUG       LWIP_DBG_ON */
/* #define TCP_DEBUG       LWIP_DBG_ON */


/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/

#define LWIP_DHCP_CHECK_LINK_UP         1


/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/
/* no Netconn API */
#define LWIP_NETCONN                    0


/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/

#define LWIP_IPV6                       0
#define IPV6_FRAG_COPYHEADER            1

#define LWIP_IPV4                       1
#define IPV4_FRAG_COPYHEADER            0
#define IP_REASSEMBLY                   0
#define IP_FRAG                         0

#ifdef __cplusplus
}
#endif

#endif /* __LWIP__LWIPOPTS_H__ */

/**
 * lwipopts.h -- lwIP configuration for macrousb (NO_SYS, raw API).
 *
 * The device runs a DHCP server and DNS server (over UDP) and an HTTP server
 * (over TCP). It is NOT a DHCP/DNS client, so those are disabled.
 * STM32F411 has 128 KiB RAM, so buffers are sized comfortably.
 */
#ifndef LWIPOPTS_H_
#define LWIPOPTS_H_

/* ---- core model ---- */
#define NO_SYS                      1
#define SYS_LIGHTWEIGHT_PROT        0
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0
#define LWIP_RAW                    0

/* ---- memory ---- */
#define MEM_LIBC_MALLOC             0
#define MEMP_MEM_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (24 * 1024)

#define MEMP_NUM_PBUF               16
#define PBUF_POOL_SIZE              8
#define MEMP_NUM_UDP_PCB            6
#define MEMP_NUM_TCP_PCB            6
#define MEMP_NUM_TCP_PCB_LISTEN     2
#define MEMP_NUM_TCP_SEG            16

/* ---- protocols ---- */
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define LWIP_DHCP                   0   /* we are the DHCP *server* */
#define LWIP_DNS                    0   /* we are the DNS *server*  */
#define LWIP_AUTOIP                 0

/* Accept DHCP server traffic (UDP :67) before the host has an IP. */
#define LWIP_IP_ACCEPT_UDP_PORT(p)  ((p) == PP_NTOHS(67))

/* ---- TCP sizing (full-speed USB, MTU 1500) ---- */
#define TCP_MSS                     (1500 - 20 - 20)
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_WND                     (4 * TCP_MSS)

/* ---- netif ---- */
#define ETH_PAD_SIZE                0
#define LWIP_SINGLE_NETIF           1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_STATUS_CALLBACK  0
#define ETHARP_SUPPORT_STATIC_ENTRIES 1

/* ---- trim unused features ---- */
#define LWIP_STATS                  0
#define LWIP_STATS_DISPLAY          0
#define LWIP_DEBUG                  0
#define LWIP_MULTICAST_PING         1
#define LWIP_BROADCAST_PING         1

#endif /* LWIPOPTS_H_ */

#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// lwIP's configuration, which is a header the *application* supplies rather than something the
// stack ships with defaults for. `repl/` needed none of this because it links
// `pico_cyw43_arch_none`; the moment a TCP/IP stack is on the link line, this file has to exist or
// the SDK's own sources will not compile.
//
// It is deliberately the smallest thing that joins a network. The demo beside it does not open a
// socket, so most of lwIP is switched off — what is left is enough to bring an interface up, take a
// DHCP lease, and let the driver report that the link is up.

// No RTOS. The `threadsafe_background` arch variant drives lwIP from an interrupt rather than from
// a thread, so the stack itself is still single-context.
#define NO_SYS                      1

// A lease is what makes a join mean something: the SDK reports the link as up only once there is an
// address, so without DHCP every join would time out on a working network.
#define LWIP_DHCP                   1

// The BSD-socket and netconn APIs both need an RTOS, which `NO_SYS` says there is not. What is left
// is the raw callback API, and nothing here uses even that yet.
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_ICMP                   1
#define LWIP_DNS                    1

// The driver asks to be told when the interface goes up or down.
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

// Checksums are computed in software here. The RP2350 has no offload for them and the CYW43439 does
// not do it on the host's behalf, so leaving these on is not a choice so much as an accounting of
// what is actually happening.
#define LWIP_CHKSUM_ALGORITHM       3

// Pool sizes. These are the SDK examples' numbers, which are sized for a couple of connections on a
// board with 520 KB of SRAM. Nothing here is tuned; if a future program runs out of pbufs, this is
// the file to look in.
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_IPV4                   1
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#endif

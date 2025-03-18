
#ifndef LWIP_HDR_LWIPOPTS_H
#define LWIP_HDR_LWIPOPTS_H

#include "tuya_iot_config.h"

#define TCPIP_THREAD_NAME              "TUYA_TCPIP"
//#define LWIP_TCPIP_CORE_LOCKING         0
#define LWIP_TCPIP_TIMEOUT              1

#define LWIP_RAND()                     tkl_system_get_random(0xFFFFFFFF)
#define LWIP_SRAND()                    

#define DEFAULT_THREAD_STACKSIZE        500


/* ---------- MTU options ---------- */
#define LWIP_TUYA_MTU                   1500

/* ---------- TCP options ---------- */
#define LWIP_TCP                        1

#define LWIP_TCP_KEEPALIVE              1
//#define LWIP_SO_SNDTIMEO                1
//#define LWIP_SO_RCVTIMEO                1
#define TCP_MSS                         (LWIP_TUYA_MTU - 40)
#define TCP_WND                         (5*TCP_MSS)

/* ---------- EAPOL ---------------- */
//#define LWIP_EAPOL_SUPPORT              1

/* ---------- UDP options ---------- */
#define LWIP_UDP                        1

/* ---------- ICMP options ---------- */
#define LWIP_ICMP                       1

/* ---------- ARP options ----------- */
#define LWIP_ARP                        1

/* ---------- DHCP options ---------- */
#define LWIP_DHCP                       1

#define DHCP_COARSE_TIMER_SECS          60
#define LWIP_DHCP_SECONDS_ELAPSE        1
#define DHCP_CREATE_RAND_XID            1


/* ---------- DNS options ---------- */
#define LWIP_DNS                        1

/* ---------- SO_REUSE options --------- */
#define SO_REUSE                        1

/* Support Multicast */
#define LWIP_IGMP                       1

/* ---------RAW option--------- */
#define LWIP_RAW                        1

/* ---------IPv6 option--------- */
//#define LWIP_IPV6                       0

#define LWIP_IPV6_DHCP6 LWIP_IPV6

/* ---------Socket options ---------*/
#define LWIP_SOCKET                     1

//#define LWIP_SO_LINGER                  1

//#define LWIP_TUYA_SO_LINGER_RST         1

//#define LWIP_PROVIDE_ERRNO              1

#define LWIP_SOCKET_OFFSET              1

/* ---------Enable Netconn API--------- */
#define LWIP_NETCONN                    1

/* ------initial port before dhcp------ */
#define LWIP_RANDOMIZE_INITIAL_LOCAL_PORTS  1

#define LWIP_TUYA_PACKET_PRINT          0
    
    
//#define LWIP_DEBUG                      0
// Debug Options
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define API_LIB_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define IGMP_DEBUG                  LWIP_DBG_OFF
#define INET_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IP_REASS_DEBUG              LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define MEM_DEBUG                   LWIP_DBG_OFF
#define MEMP_DEBUG                  LWIP_DBG_OFF
#define SYS_DEBUG                   LWIP_DBG_OFF
#define TIMERS_DEBUG                LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define SLIP_DEBUG                  LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF
#define AUTOIP_DEBUG                LWIP_DBG_OFF
#define DNS_DEBUG                   LWIP_DBG_OFF
#define IP6_DEBUG                   LWIP_DBG_OFF
    
#define ETHARP_DEBUG                LWIP_DBG_OFF
#define UDP_LPC_EMAC                LWIP_DBG_OFF
#define ETHEAPOL_DEBUG              LWIP_DBG_ON
    
    
#ifdef LWIP_DEBUG
#define MEMP_OVERFLOW_CHECK         1
#define MEMP_SANITY_CHECK           1
#define LWIP_DBG_TYPES_ON           LWIP_DBG_ON
#define LWIP_DBG_MIN_LEVEL          LWIP_DBG_LEVEL_ALL
#else
#define LWIP_NOASSERT               0
#define LWIP_STATS                  0
#endif
    
#if LWIP_STATS
#define TCPIP_THREAD_STACKSIZE          (4096*2)
    
#define LINK_STATS                      1
#define ETHARP_STATS                    1
#define IP_STATS                        1
#define IPFRAG_STATS                    1
#define ICMP_STATS                      1
#define IGMP_STATS                      1
#define UDP_STATS                       1
#define TCP_STATS                       1
#define MEM_STATS                       1
#define MEMP_STATS                      1
#define SYS_STATS                       1
#define LWIP_STATS_DISPLAY              1
#define IP6_STATS                       1
#define ICMP6_STATS                     1
#define IP6_FRAG_STATS                  1
#define MLD6_STATS                      1
#define ND6_STATS                       1
#define MIB2_STATS                      1
#define MIB2_STATS                      1
    
#define TUYA_ETHERNETIF_STATS           1
#endif


#include "lwip/init.h"


//#define TCPIP_THREAD_STACKSIZE          (1024*4)
//#define TCPIP_THREAD_PRIO               (11 - 2)

//#define DHCPC_THREAD_STACKSIZE          (1024*2)

//#define DHCPC_THREAD_PRIO               5

#define LWIP_COMPAT_MUTEX               1

#define MEM_ALIGNMENT                   4

#define LWIP_CHKSUM_ALGORITHM           3

#define LWIP_NETIF_API                  1

//#define LWIP_TX_PBUF_ZERO_COPY 		1

//#define LWIP_DHCP_CHECK_LINK_UP         0

//#define CONFIG_TUYA_SOCK_SHIM 1

//#define LWIP_NETIF_HOSTNAME 1

//#define LWIP_CHKSUM(buf, len) tkl_ethernetif_ip_chksum(buf, len)

#define LWIP_HOOK_IP4_ROUTE_SRC(s, d)         (void *)ip4_route_src_hook(s, d)

#define LWIP_DHCP_DISCOVER_RETRY_INTERVAL_1S 1

//#define SOCK_API_SYNC 1

//#define LWIP_NETCONN_SEM_PER_THREAD 1

#define MEMP_MEM_MALLOC 1

//#define LWIP_DHCPC_STATIC_IPADDR_ENABLE 0

#define LWIP_CONFIG_FAST_DHCP 1

#endif /* LWIP_HDR_LWIPOPTS_H */

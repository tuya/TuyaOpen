/**
 * @file ethernetif.c
 * @brief Ethernet interface management functions for Tuya devices.
 *
 * This file provides the implementation of Ethernet interface management,
 * including initialization, IP and MAC address configuration, packet
 * transmission and reception, and utility functions for Tuya devices.
 * It integrates with the lwIP stack to handle Ethernet frames and manage
 * network interfaces.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/err.h"
#include "ethernetif.h"
#include "lwip_init.h"
#include "lwip/ethip6.h" //Add for ipv6
#include "lwip/dns.h"
#include "tkl_lwip.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#if LWIP_TUYA_PACKET_PRINT
#define TUYA_PACKET_PRINT(pbuf) tuya_ethernetif_packet_print(pbuf)
#else
#define TUYA_PACKET_PRINT(pbuf)
#endif

/***********************************************************
*************************variable define********************
***********************************************************/
/* network interface structure */
//struct netif xnetif[NETIF_NUM];

#if LWIP_TUYA_PACKET_PRINT
/***********************************************************
*************************function define********************
***********************************************************/
/**
 * @brief ethernetif packet print, enable/disable by LWIP_TUYA_PACKET_PRINT
 *
 * @param[in]       p       the packet of pbuf
 * @return  void
 */
static void tuya_ethernetif_packet_print(struct pbuf *p)
{
    u32_t i, timeout, hour, minute, second, msecond;
    struct pbuf *q;

    timeout = sys_now() % 86400000;
    hour = timeout / 1000 / 60 / 60;
    minute = (timeout / 1000 / 60) % 60;
    second = (timeout / 1000) % 60;
    msecond = timeout % 1000;
    printf("+---------+---------------+----------+\r\n");
    printf("%02d:%02d:%02d,%d,000   ETHER\r\n", hour, minute , second, msecond);
    printf("|0   |");
    for (q = p; q != NULL; q = q->next) {
        for (i = 0; i < q->len; i++) {
            printf("%02x|", ((u8_t *)q->payload)[i]);
        }
    }
    printf("\r\n\n\n");
}
#endif /* LWIP_TUYA_PACKET_PRINT */

/**
 * @brief get netif by index
 *
 * @param[in]       net_if_idx    the num of netif index
 * @return  NULL: get netif fail   other: the point of netif
 */
struct netif *tuya_ethernetif_get_netif_by_index(const TUYA_NETIF_TYPE net_if_idx)
{
    if (net_if_idx > (NETIF_NUM - 1)) {
        return NULL;
    }

    return tkl_lwip_get_netif_by_index(net_if_idx);
}

/**
 * @brief get netif ipaddr from lwip
 *
 * @param[in]       net_if_idx    index of netif
 * @param[in]       type          ip type
 * @param[out]      ip            ip of netif(ip gateway mask)
 * @return  0 on success
 */
int tuya_ethernetif_get_ip(const TUYA_NETIF_TYPE net_if_idx, NW_IP_TYPE type, NW_IP_S *ip)
{
#if 0
    struct netif *pnetif = tuya_ethernetif_get_netif_by_index(net_if_idx);
    if(NULL == pnetif) {
        return -1;
    }
#if LWIP_IPV6
    int i = 0;
#endif

    if(type == NW_IPV4) {
        if(!ip_addr_isany_val(pnetif->ip_addr)) {
            ip->type = (IP_ADDR_TYPE)TY_AF_INET;
            ip->addr.ip4.islinklocal = 0;

            ip4addr_ntoa_r(ip_2_ip4(&pnetif->ip_addr), ip->addr.ip4.ip, 16);
            ip4addr_ntoa_r(ip_2_ip4(&pnetif->gw), ip->addr.ip4.gw, 16);
            ip4addr_ntoa_r(ip_2_ip4(&pnetif->netmask), ip->addr.ip4.mask, 16);

            if((ip_addr_get_ip4_u32(&pnetif->ip_addr)  & 0xffff0000) == 0xA96FE0000) {
                ip->addr.ip4.islinklocal = 1;
            }

            return 0;
        }
    }
    #if LWIP_IPV6
    else if(type == NW_IPV6_LL) {
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
          if (ip6_addr_isvalid(netif_ip6_addr_state(pnetif, i)) &&
              ip6_addr_islinklocal(netif_ip6_addr(pnetif, i))) {
            ip->type = (IP_ADDR_TYPE)TY_AF_INET6;
            ip6addr_ntoa_r(netif_ip6_addr(pnetif, i), ip->addr.ip6.ip, 40);
            ip->addr.ip6.islinklocal = 1;

            return 0;
          }
        }
    }else if(type == NW_IPV6) {
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
          if (ip6_addr_isvalid(netif_ip6_addr_state(pnetif, i)) &&
              !ip6_addr_islinklocal(netif_ip6_addr(pnetif, i))) {
            ip->type = (IP_ADDR_TYPE)TY_AF_INET6;
            ip6addr_ntoa_r(netif_ip6_addr(pnetif, i), ip->addr.ip6.ip, 40);
            ip->addr.ip6.islinklocal = 0;

            return 0;
          }
        }
    }
    #endif
#endif

    return -1;
}
/**
 * @brief set netif's mac
 *
 * @param[in]       net_if_idx    index of netif
 * @param[in]       mac           mac to set
 * @return  int    OPRT_OK:success   other:fail
 */
int tuya_ethernetif_mac_set(const TUYA_NETIF_TYPE net_if_idx, NW_MAC_S *mac)
{
    struct netif *pnetif = NULL;
    u32_t i = 0;

    if(MAC_ADDR_LEN != NETIF_MAX_HWADDR_LEN){
        return OPRT_OS_ADAPTER_NOT_SUPPORTED;
    }

    pnetif = tuya_ethernetif_get_netif_by_index(net_if_idx);

    for (i = 0; i < MAC_ADDR_LEN; i++) {
        pnetif->hwaddr[i] = mac->mac[i];
    }

    return OPRT_OK;
}

/**
 * @brief get netif's mac
 *
 * @param[in]       net_if_idx    index of netif
 * @param[out]      mac           mac to set
 * @return  int    OPRT_OK:success   other:fail
 */
int tuya_ethernetif_mac_get(const TUYA_NETIF_TYPE net_if_idx, NW_MAC_S *mac)
{
    struct netif *pnetif = NULL;
    u32_t i = 0;

    if(MAC_ADDR_LEN != NETIF_MAX_HWADDR_LEN){
        return OPRT_OS_ADAPTER_NOT_SUPPORTED;
    }

    pnetif = tuya_ethernetif_get_netif_by_index(net_if_idx);

    for (i = 0; i < MAC_ADDR_LEN; i++) {
        mac->mac[i] = pnetif->hwaddr[i];
    }

    return OPRT_OK;
}

#if not_yet
/**
 * @brief netif check(check netif is up/down and ip is valid)
 *
 * @param   void
 * @return  int    OPRT_OK:netif is up and ip is valid
 */
int tuya_ethernetif_station_state_get(void)
{
    struct netif *pnetif = NULL;

    pnetif = tuya_ethernetif_get_netif_by_index(NETIF_STA_IDX);

    if (!netif_is_up(pnetif)) {
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    if (ip4_addr_isany_val(*(netif_ip_addr4(pnetif)))) {
        return OPRT_OS_ADAPTER_COM_ERROR;
    }

    return OPRT_OK;
}
#endif

/**
 * @brief ethernetif int
 *
 * @param[in]      netif     the netif to be inited
 * @return  void
 */
static void tuya_ethernet_init(struct netif *netif)
{

    /* set netif MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set netif maximum transfer unit */
    netif->mtu = LWIP_TUYA_MTU;

    /* Accept broadcast address and ARP traffic */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

#if LWIP_IGMP
    /* make LwIP_Init do igmp_start to add group 224.0.0.1 */
    netif->flags |= NETIF_FLAG_IGMP;
#endif

#if LWIP_IPV6 && LWIP_IPV6_MLD
    netif->flags |= NETIF_FLAG_MLD6;
#endif

    /* Wlan interface is initialized later */
    tkl_ethernetif_init(netif);
}

/**
 * @brief ethernet interface sendout the pbuf packet
 *
 * @param[in]      netif     the netif to which to be inited
 * @return  err_t  SEE "err_enum_t" in "lwip/err.h" to see the lwip err(ERR_OK: SUCCESS other:fail)
 */
err_t tuya_ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    if (netif->name[1] == '0') {
        netif->hostname = "lwip0";
    } else if (netif->name[1] == '1') {
        netif->hostname = "lwip1";
    }
#endif /* LWIP_NETIF_HOSTNAME */

    netif->output = etharp_output;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif
    //netif->linkoutput = tuya_ethernetif_output;
    netif->linkoutput = tkl_ethernetif_output;

    /* initialize the hardware */
    tuya_ethernet_init(netif);

    etharp_init();

    return ERR_OK;
}

int tuya_ethernetif_get_ifindex_by_mac(NW_MAC_S *mac, TUYA_NETIF_TYPE *net_if_idx)
{
    int i;
    struct netif *netif;

    if (NULL == mac || NULL == net_if_idx) {
        return -1;
    }

    for (i = 0; i < NETIF_NUM; i++) {
        netif = tuya_ethernetif_get_netif_by_index(i);
        if (NULL == netif) {
            continue;
        }

        if (memcmp(netif->hwaddr, mac->mac, 6) == 0) {
            *net_if_idx = i;
            break;
        }
    }

    return 0;
}

/**
 * @brief get DNS server from lwip
 *
 * @param[in]       net_if_idx    index of netif
 * @param[in]       type          DNS server type
 * @param[out]      ip            IP address of DNS server
 * @return  0 on success
 */
int tuya_ethernetif_get_dns_srv(NW_IP_TYPE type, NW_IP_S *ip)
{
    ip_addr_t *dns_srv;

    for (int i = 0; i < DNS_MAX_SERVERS; i++) {
        dns_srv = (ip_addr_t *)dns_getserver(i);
        if (IPADDR_TYPE_V4 == IP_GET_TYPE(dns_srv)) {
            ip4addr_ntoa_r(ip_2_ip4(dns_srv), ip->ip, 16);
            break;
        }
    }
    return 0;
}

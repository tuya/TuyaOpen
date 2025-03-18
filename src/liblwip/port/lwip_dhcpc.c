/**
 * @file lwip_dhcpc.c
 * @brief DHCP client implementation for LwIP.
 *
 * This file provides the DHCP client functionality, allowing devices to obtain IP addresses
 * and other network configuration details from a DHCP server. It includes mechanisms for
 * IP address renewal and rebinding, as well as support for fast DHCP based on predefined
 * network parameters.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "ethernetif.h"
#include "lwip_dhcpc.h"
#include "lwip/tcpip.h"
#include "lwip_init.h"
#include "netifapi.h"
#include "tal_log.h"
#include "tal_workq_service.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************variable define********************
***********************************************************/
struct ip_info {
    void (*cb)(uint8_t event, void *arg);
};//�Ȱ���ֻ��һ��netif������������Ҫ���Ӹ���netif���������Ӧ��last ip��Ϣ

struct ip_info g_ip_info = {0};
#if LWIP_CONFIG_FAST_DHCP
extern u8_t is_fast_dhcp;
extern ip_addr_t offered_ip_addr;
extern ip_addr_t offered_mask_addr;
extern ip_addr_t offered_gw_addr;
extern ip_addr_t offered_dns_addr;
#endif /* LWIP_CONFIG_FAST_DHCP */
/***********************************************************
*************************function define********************
***********************************************************/
static void __dhcp_callback(struct netif *netif, uint8_t event, uint8_t isup)
{
    if(!netif) {
        return;
    }

    switch (event) {
        case 0://dhcpv4
        if(isup) {
            PR_DEBUG("ipv4 addr: %s", ip4addr_ntoa(ip_2_ip4(&(netif->ip_addr))));
            g_ip_info.cb(IPV4_DHCP_SUCC, NULL);
        }else {
            g_ip_info.cb(IPV4_DHCP_FAIL, NULL);
        }
        break;

        #if LWIP_IPV6
        case 1://ipv6 ll
        if(isup) {
            PR_DEBUG("ipv6 ll addr: %s", ip6addr_ntoa(ip_2_ip6(&(netif->ip6_addr[0]))));
            g_ip_info.cb(IPV6_LL_SUCC, NULL);
        }else {
            g_ip_info.cb(IPV6_LL_FAIL, NULL);
        }
        break;

        case 2://ipv6 globe
        if(isup) {
            PR_DEBUG("ipv6 global addr: %s", ip6addr_ntoa(ip_2_ip6(&(netif->ip6_addr[1]))));
            g_ip_info.cb(IPV6_DHCP_SUCC, NULL);
        }else {
            g_ip_info.cb(IPV6_DHCP_FAIL, NULL);
        }
        break;
        #endif

        default:
        break;
    }

    return;
}

/**
 * @brief start client dhcp  to get ip for WIFI STATION mode
 *
 * @param     cb : event callback
 * @param     ip_info : fast dhcp ip info, normal dhcp if NULL
 * @return    0:success other:fail
 */
int tuya_dhcpv4_client_start(void (*cb)(LWIP_EVENT_E event, void *arg), FAST_DHCP_INFO_T *ip_info)
{
#if LWIP_CONFIG_FAST_DHCP
    struct dhcp *dhcp;
#endif /* LWIP_CONFIG_FAST_DHCP */

    if (!cb) {
        return -2;
    }

    struct netif *pnetif = tuya_ethernetif_get_netif_by_index(NETIF_STA_IDX);
    if(!pnetif) {
        return -1;
    }

#if LWIP_CONFIG_FAST_DHCP
    if (    (NULL != ip_info)
         && (ip_info->ip[0] != '\0')
         && (ip_info->mask[0] != '\0')
         && (ip_info->gw[0] != '\0')
         && (ip_info->dns[0] != '\0')) {
        is_fast_dhcp = 1;
        ip_addr_t *p_ip_addr = &offered_ip_addr;
        IP_SET_TYPE(p_ip_addr, IPADDR_TYPE_V4);
        ip4addr_aton(ip_info->ip, ip_2_ip4(p_ip_addr));

        ip_addr_t *p_mask_addr = &offered_mask_addr;
        IP_SET_TYPE(p_mask_addr, IPADDR_TYPE_V4);
        ip4addr_aton(ip_info->mask, ip_2_ip4(p_mask_addr));

        ip_addr_t *p_gw_addr = &offered_gw_addr;
        IP_SET_TYPE(p_gw_addr, IPADDR_TYPE_V4);
        ip4addr_aton(ip_info->gw, ip_2_ip4(p_gw_addr));

        ip_addr_t *p_dns_addr = &offered_dns_addr;
        IP_SET_TYPE(p_dns_addr, IPADDR_TYPE_V4);
        ip4addr_aton(ip_info->dns, ip_2_ip4(p_dns_addr));

        netifapi_netif_set_addr(pnetif, (ip4_addr_t *)p_ip_addr, (ip4_addr_t *)p_mask_addr, (ip4_addr_t *)p_gw_addr);
        dns_setserver(0, p_dns_addr);
    } else {
        /* clear ip address of fast DHCP for TuyaOS's network backup feature */
        is_fast_dhcp = 0;
        ip_addr_set_zero(&offered_ip_addr);
        ip_addr_set_zero(&offered_mask_addr);
        ip_addr_set_zero(&offered_gw_addr);
        ip_addr_set_zero(&offered_dns_addr);
        netifapi_netif_set_addr(pnetif, (ip4_addr_t *)&offered_ip_addr, (ip4_addr_t *)&offered_mask_addr, (ip4_addr_t *)&offered_gw_addr);

        dhcp = netif_dhcp_data(pnetif);
        if (NULL != dhcp) {
            ip_addr_set_zero(&dhcp->server_ip_addr);
            ip4_addr_set_zero(&dhcp->offered_ip_addr);
            ip4_addr_set_zero(&dhcp->offered_sn_mask);
            ip4_addr_set_zero(&dhcp->offered_gw_addr);
        }
    }
#endif /* LWIP_CONFIG_FAST_DHCP */


    if (!netif_is_up(pnetif)) {
        netifapi_netif_set_up(pnetif);
    }

    #if LWIP_IPV6
    static u8_t lcaddrfg = 0;
    if(!lcaddrfg) {
        netif_create_ip6_linklocal_address(pnetif, 1);
        lcaddrfg = 1;
    }
    #endif

    #if DHCP_CREATE_RAND_XID && defined(LWIP_SRAND)
    /* For each system startup, fill in a random seed with different system ticks. */
    LWIP_SRAND();
    #endif /* DHCP_CREATE_RAND_XID && defined(LWIP_SRAND) */

    g_ip_info.cb = cb;

    netif_set_dhcp_cb(pnetif, __dhcp_callback);

    netifapi_dhcp_start(pnetif);

    return 0;
}

int *tmp_param[2] = {0};
void workqueue_dhcp(void *data)
{
    tuya_dhcpv4_client_start(tmp_param[0] , tmp_param[1] );
}

void tuya_dhcpv4_client_start_by_wq(void (*cb)(LWIP_EVENT_E event, void *arg), FAST_DHCP_INFO_T *ip_info)
{
    tmp_param[0] = cb;
    tmp_param[1] = ip_info;
    tal_workq_schedule(WORKQ_HIGHTPRI, workqueue_dhcp, tmp_param);
}

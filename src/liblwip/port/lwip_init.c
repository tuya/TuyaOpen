/**
 * @file lwip_init.c
 * @brief Initialization routines for LwIP on platforms.
 *
 * This file contains the implementation of the initialization process for the LwIP TCP/IP stack
 * on Tuya platforms. It includes setting up network interfaces, configuring IP addresses,
 * and initializing the LwIP core functions. It supports both static and dynamic IP address
 * configuration and is designed to work with Tuya's Ethernet interface management module.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
// #include "oshal.h"

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "ethernetif.h"
#include "lwip_init.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
#ifdef TUYA_SDK_CLI_ADAPTER
#include "tuya_cli_adapt.h"
#endif
#include "tal_wifi.h"
#include "tal_log.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
*************************variable define********************
***********************************************************/

/***********************************************************
*************************function define********************
***********************************************************/
/**
 * @brief tuya lwip init
 *
 * @param     void
 * @return    void
 */
extern void cli_printf(const char *f, ...);

void TUYA_LwIP_Init(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;
    s8_t idx = 0;
    struct netif *pnetif = NULL;

    // The WiFi STATION/AP must correspond to LWIP's netif0/1
    if (((int)NETIF_AP_IDX != (int)WF_AP) || ((int)NETIF_STA_IDX != (int)WF_STATION)) {
        // os_printf(LM_APP, LL_INFO, "wifi station/ap does not match
        // netif0/1\n");
    }

    // Initialize LWIP core and resources
    tcpip_init(NULL, NULL);

    // Initialize netif, set IP address and name
    for (idx = 0; idx < NETIF_NUM; idx++) {
#if LWIP_DHCPC_STATIC_IPADDR_ENABLE
        if (idx == 0) {
            IP4_ADDR(ip_2_ip4(&ipaddr), IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
            IP4_ADDR(ip_2_ip4(&netmask), NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
            IP4_ADDR(ip_2_ip4(&gw), GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
        } else {
            IP4_ADDR(ip_2_ip4(&ipaddr), AP_IP_ADDR0, AP_IP_ADDR1, AP_IP_ADDR2, AP_IP_ADDR3);
            IP4_ADDR(ip_2_ip4(&netmask), AP_NETMASK_ADDR0, AP_NETMASK_ADDR1, AP_NETMASK_ADDR2, AP_NETMASK_ADDR3);
            IP4_ADDR(ip_2_ip4(&gw), AP_GW_ADDR0, AP_GW_ADDR1, AP_GW_ADDR2, AP_GW_ADDR3);
        }
#else
        IP4_ADDR(ip_2_ip4(&ipaddr), 0, 0, 0, 0);
        IP4_ADDR(ip_2_ip4(&netmask), 0, 0, 0, 0);
        IP4_ADDR(ip_2_ip4(&gw), 0, 0, 0, 0);
#endif
        pnetif = tuya_ethernetif_get_netif_by_index(idx);
#ifdef LWIP_DUAL_NET_SUPPORT
        if (idx == NETIF_ETH_IDX) {
            pnetif->name[0] = 'e';
            pnetif->name[1] = 'n';
            netif_add(pnetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), ip_2_ip4(&gw), NULL, &ethernetif_init,
                      &ethernet_input);
        } else {
#endif
            pnetif->name[0] = 'r';
            pnetif->name[1] = '0' + idx;

            netif_add(pnetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), ip_2_ip4(&gw), NULL, &tuya_ethernetif_init,
                      &tcpip_input);
#ifdef LWIP_DUAL_NET_SUPPORT
        }
#endif
        PR_DEBUG("interface %d is initialized", idx);
    }

    // Set the netif for STATION mode as the default netif
    netif_set_default(tuya_ethernetif_get_netif_by_index(NETIF_STA_IDX));

    /***************************************/
    /* Note: netif_set_up/down is operated in WiFi */
    /***************************************/
    for (idx = 0; idx < NETIF_NUM; idx++) {
        pnetif = tuya_ethernetif_get_netif_by_index(idx);
        if (pnetif) {
            netif_set_up(pnetif);
            netif_set_flags(pnetif, NETIF_FLAG_LINK_UP);
        }
    }
}


#ifndef __LWIP_DHCPC_H
#define __LWIP_DHCPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/tcpip.h"
#include "lwip/init.h" //for lwip version control
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "tkl_wifi.h"

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
 * @brief start client dhcp  to get ip for WIFI STATION mode
 *
 * @param     cb : event callback
 * @param     ip_info : fast dhcp ip info, normal dhcp if NULL
 * @return    0:success other:fail
 */
int tuya_dhcpv4_client_start(void (*cb)(LWIP_EVENT_E event, void *arg), FAST_DHCP_INFO_T *ip_info);

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_DHCPC_H */

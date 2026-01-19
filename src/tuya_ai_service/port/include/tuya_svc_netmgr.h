/**
 * @file tuya_svc_netmgr.h
 * @brief tuya_svc_netmgr module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_SVC_NETMGR_H__
#define __TUYA_SVC_NETMGR_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"
#include "tuya_svc_netmgr_linkage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/**
 * @brief Definition of network status
 */
typedef uint8_t NETWORK_STATUS_E;
#define NETWORK_STATUS_OFFLINE  0  // all linkages are down
#define NETWORK_STATUS_LOCAL    1  // any linkage is up
#define NETWORK_STATUS_MQTT     2  // MQTT is connected

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Get network status
 *
 * @return see NETWORK_STATUS_E for details
 */
NETWORK_STATUS_E tuya_svc_netmgr_get_status(VOID);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_SVC_NETMGR_H__ */

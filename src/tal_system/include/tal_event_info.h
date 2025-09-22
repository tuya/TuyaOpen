/**
 * @file tal_event_info.h
 * @brief Event information definitions for Tuya Open Link Up Switch system.
 *
 * This header file defines various event macros used for device health,
 * network status, and system operations within the Tuya Open Link Up Switch project.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     add EVENT_LINK_TYPE_CHG
 *
 */

#ifndef __TAL_EVENT_INFO_H__
#define __TAL_EVENT_INFO_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define EVENT_REBOOT_REQ                                                                                               \
    "dev.reboot.req" // device health check reboot request, application should subscribe it if needed
#define EVENT_REBOOT_ACK        "dev.reboot.ack" // device health check reboot ack, application should publish when it ready
#define EVENT_LAN_CLIENT_CLOSE  "lan.cli.close" // lan client close
#define EVENT_RSC_UPDATE        "rsc.update"    // register center changed
#define EVENT_HEALTH_ALERT      "health.alert"  // health alert
#define EVENT_LINK_STATUS_CHG   "link.status"   // link status change
#define EVENT_LINK_TYPE_CHG     "link.type"     // link conn change
#define EVENT_RESET             "dev.reset"     // device reset
#define EVENT_MQTT_CONNECTED    "mqtt.con"      // mqtt connect
#define EVENT_MQTT_DISCONNECTED "mqtt.disc"     // mqtt disconnect
#define EVENT_LINK_ACTIVATE     "link.activate" // linkage got activate info

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __TAL_EVENT_INFO_H__ */

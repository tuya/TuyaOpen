/**
 * @file tal_event_info.h
 * @brief Defines system-wide events for Tuya IoT applications.
 *
 * This header file lists constants for various system and application events
 * that can be used within Tuya IoT applications to handle specific actions like
 * device reboot requests, LAN client disconnections, MQTT connectivity changes,
 * and more. These events facilitate communication and state management across
 * different components of Tuya IoT applications, enabling a responsive and
 * dynamic environment.
 *
 * Applications can subscribe to or publish these events as needed, allowing for
 * modular and decoupled design. This file is an essential part of the Tuya IoT
 * Development Platform, providing developers with predefined events that cover
 * common IoT scenarios.
 *
 * @note This file should be included in projects that require interaction with
 * Tuya IoT Platform events.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BASE_EVENT_INFO_H__
#define __BASE_EVENT_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief the event define
 *
 */
#define EVENT_REBOOT_REQ                                                                                               \
    "dev.reboot.req" // device health check reboot request, application should
                     // subscribe it if needed
#define EVENT_REBOOT_ACK                                                                                               \
    "dev.reboot.ack"                            // device health check reboot ack, application should
                                                // publish when it ready
#define EVENT_LAN_CLIENT_CLOSE  "lan.cli.close" // lan client close
#define EVENT_RSC_UPDATE        "rsc.update"    // register center changed
#define EVENT_HEALTH_ALERT      "health.alert"  // health alert
#define EVENT_LINK_STATUS_CHG   "link.status"   // link status change
#define EVENT_LINK_TYPE_CHG     "link.type"     // link conn change
#define EVENT_RESET             "dev.reset"     // device reset
#define EVENT_MQTT_CONNECTED    "mqtt.con"      // mqtt connect
#define EVENT_MQTT_DISCONNECTED "mqtt.disc"     // mqtt disconnect
#define EVENT_LINK_ACTIVATE     "link.activate" // linkage got activate info

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__BASE_EVENT_H__ */

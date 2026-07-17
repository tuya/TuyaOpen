/**
 * @file dev_evt.h
 * @brief Device network-operation event notification (ported from tuyaos-ai
 *        gw_dev_evt mechanism).
 *
 * The SDK network paths (DNS/TCP/MQTT connect/publish, DP report, OTA, STA
 * connect, reset...) call tuya_dev_evt_notify() with ACTION_BEFORE right before
 * the operation and ACTION_AFTER right after it. A single consumer (registered
 * via tuya_dev_evt_set_cb) can then keep the device awake for the duration of
 * each operation - this is how the ULP power manager (lpmgr) knows to hold a
 * wakelock around each network activity instead of only around the connection
 * lifecycle.
 *
 * Placed in the leaf "common" component so every layer (from tal_network up to
 * the cloud service and the tal_wifi_ulp subscriber) can both notify and
 * subscribe without introducing a circular dependency.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __DEV_EVT_H__
#define __DEV_EVT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device network-operation event id.
 *
 * Kept identical (order and names) to the tuyaos-ai DEV_EVT_E so the ULP
 * consumer logic can be ported verbatim. Note: DEV_EVT_TLS_CONNECT and
 * DEV_EVT_HTTP_CONNECT are defined for parity but not fired - the DNS/TCP
 * notify points at the tal_network layer already cover every TLS/HTTP/MQTT/ATOP
 * socket setup.
 */
typedef enum {
    DEV_EVT_STA_CONNECT,
    DEV_EVT_TCP_CONNECT,
    DEV_EVT_TLS_CONNECT,
    DEV_EVT_HTTP_CONNECT,
    DEV_EVT_MQTT_CONNECT,
    DEV_EVT_MQTT_PUBLISH,
    DEV_EVT_MQTT_PING,
    DEV_EVT_DNS_LOOKUP,
    DEV_EVT_BLE_STACK,
    DEV_EVT_DP_PROCESS,
    DEV_EVT_OTA,
    DEV_EVT_RESET,

    DEV_EVT_DEFAULT
} DEV_EVT_E;

typedef enum {
    ACTION_BEFORE,
    ACTION_AFTER
} DEV_ACTION_E;

typedef void (*DEV_EVT_CB)(DEV_EVT_E evt, DEV_ACTION_E action, void *ctx);

/**
 * @brief Register the single device-event consumer.
 *
 * @param[in] cb consumer callback (NULL to unregister).
 */
void tuya_dev_evt_set_cb(DEV_EVT_CB cb);

/**
 * @brief Notify the registered consumer that a network operation is starting
 *        (ACTION_BEFORE) or finishing (ACTION_AFTER). No-op if no consumer.
 *
 * Safe to call from any context and to nest (e.g. HTTP -> DNS -> TCP).
 *
 * @param[in] evt    operation id
 * @param[in] action ACTION_BEFORE / ACTION_AFTER
 * @param[in] ctx    optional context, may be NULL
 */
void tuya_dev_evt_notify(DEV_EVT_E evt, DEV_ACTION_E action, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_EVT_H__ */

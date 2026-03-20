/**
 * @file tuya_device_timer.h
 * @brief tuya_device_timer module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_DEVICE_TIMER_H__
#define __TUYA_DEVICE_TIMER_H__

#include "tuya_cloud_types.h"
#include "mqtt_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the timer task module.
 *
 * Load persisted timers from flash, register MQTT handlers for protocol 22,
 * and start the periodic execution check timer.
 *
 * Call this after TUYA_EVENT_MQTT_CONNECTED.
 *
 * @param mqctx      MQTT context (borrowed, not owned by this module).
 * @param execute_cb Callback invoked when a timer fires. Must not be NULL.
 * @param user_data  Passed through to execute_cb.
 * @return 0 on success, negative error code on failure.
 */
int tuya_device_timer_init(void);

/**
 * @brief Deinitialize the timer task module.
 *
 * Stop all timers, release memory, and unregister MQTT handlers.
 * Call on device reset or before re-initialization.
 */
void tuya_device_timer_deinit(void);

/**
 * @brief Request a full timer sync from the cloud.
 *
 * Sends a timer_full_syn request (mqtt protocol 22). The cloud will respond
 * with all timers in one or more timer_syn packets (operateType=2).
 *
 * Typically called once after MQTT connects.
 */
void tuya_device_timer_full_sync_req(void);

/**
 * @brief Dump all device timers to log output.
 *
 * Prints a formatted summary of every timer item including its type
 * (one-shot / recurring), schedule, status, and action (dps).
 * Useful for debugging and diagnostics.
 */
void tuya_device_timer_dump(void);

/**
 * @brief Delete persisted timer tasks from KV storage and clear the in-memory list.
 *
 * @return OPRT_OK on success, other values on failure.
 */
OPERATE_RET tuya_device_timer_kv_delete(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_DEVICE_TIMER_H__ */

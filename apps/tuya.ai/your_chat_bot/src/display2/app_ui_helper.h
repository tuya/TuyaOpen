/**
 * @file app_ui_helper.h
 * @brief app_ui_helper module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_UI_HELPER_H__
#define __APP_UI_HELPER_H__

#include "tuya_cloud_types.h"

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
 * @brief Get the current volume value
 * @return Current volume value (0-100)
 */
uint8_t app_ui_get_volume_value(void);

/**
 * @brief Set the volume value with delay
 * @param[in] value Volume value to set (0-100)
 * @note The volume will be set after 300ms delay
 */
void app_ui_set_volume_value(uint8_t value);

/**
 * @brief Get the current date (year, month, day)
 * @param[out] year Pointer to store the year value
 * @param[out] month Pointer to store the month value (1-12)
 * @param[out] day Pointer to store the day value (1-31)
 * @note If time is not synced, this function will subscribe to time sync event
 *       and return without setting the values
 */
void app_ui_get_date(uint32_t *year, uint32_t *month, uint32_t *day);

/**
 * @brief Get the current time (hour, minute)
 * @param[out] hour Pointer to store the hour value (0-23)
 * @param[out] minute Pointer to store the minute value (0-59)
 */
void app_ui_get_time(uint32_t *hour, uint32_t *minute);

/**
 * @brief Get the WiFi connection status
 * @param[out] status Pointer to store the WiFi status (1: connected, 0: disconnected)
 */
void app_ui_get_wifi_status(uint8_t *status);

/**
 * @brief Subscribe to network status change events
 * @note When network status changes, the UI will be updated automatically
 */
void app_ui_network_status_change_subscribe(void);

/**
 * @brief Unsubscribe from network status change events
 */
void app_ui_network_status_change_unsubscribe(void);

/**
 * @brief Start the date and time update loop
 * @note This function will start a timer to update date and time periodically
 *       The update interval is calculated to trigger at the start of each minute
 */
void app_ui_get_date_time_loop_start(void);

/**
 * @brief Stop the date and time update loop
 */
void app_ui_get_date_time_loop_stop(void);

/**
 * @brief Reset the device
 * @note This will trigger a device reset through Tuya IoT client
 */
void app_ui_reset_device(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_HELPER_H__ */

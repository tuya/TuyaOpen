/**
 * @file ble_wifi_list.h
 * @brief Defines the BLE WiFi hotspot list query channel interface.
 *
 * This header file declares the callback registered on the BLE transparent
 * channel used by the app to query the WiFi hotspot list scanned by the
 * device.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_WIFI_LIST_H__
#define __BLE_WIFI_LIST_H__

#include "ble_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handles the WiFi list query request from the app.
 *
 * @param data The request payload received on the channel.
 * @param user_data Private data bound to the channel.
 */
void ble_wifi_list_query(void *data, uint32_t len, void *user_data);

#ifdef __cplusplus
}
#endif

#endif

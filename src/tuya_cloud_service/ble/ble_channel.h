/**
 * @file ble_channel.h
 * @brief Defines the BLE channel management interface for Tuya's BLE SDK.
 *
 * This header file provides the definitions and function prototypes for
 * managing BLE channels within the Tuya BLE SDK. It includes the declaration of
 * the BLE channel type enumeration, function pointer types for channel
 * callbacks, and the API functions for adding, deleting, and processing BLE
 * channels. These channels facilitate the structured communication and data
 * exchange between the BLE device and its connected peers, supporting specific
 * functionalities like network configuration. The API defined in this file is
 * essential for developers working on BLE-based applications that require
 * efficient and organized data transmission mechanisms.
 *
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_CHANNEL_H__
#define __BLE_CHANNEL_H__

#include "ble_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_CHANNLE_NETCFG = 1,
    BLE_CHANNEL_MAX,
} ble_channel_type_t;

typedef void (*ble_channel_fn_t)(void *data, void *user_data);

/**
 * @brief Adds a BLE channel.
 *
 * This function adds a BLE channel of the specified type and associates it with
 * the provided callback function.
 *
 * @param type The type of the BLE channel.
 * @param fn The callback function to be associated with the BLE channel.
 * @param priv_data A pointer to private data that will be passed to the
 * callback function.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int ble_channel_add(ble_channel_type_t type, ble_channel_fn_t fn, void *priv_data);
int ble_channel_del(ble_channel_type_t type);

/**
 * @brief Processes the BLE session channel.
 *
 * This function is responsible for processing the BLE session channel by
 * handling the specified request packet.
 *
 * @param req The request packet to be processed.
 * @param priv_data A pointer to private data associated with the BLE session
 * channel.
 */
void ble_session_channel_process(ble_packet_t *req, void *priv_data);
void ble_channel_rx_reset(void);
void ble_channel_tx_reset(void);
void ble_channel_reset(void);
int ble_channle_ack(uint16_t type, uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif

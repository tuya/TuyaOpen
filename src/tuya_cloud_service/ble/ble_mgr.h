/**
 * @file ble_mgr.h
 * @brief Header file for BLE Manager.
 *        This file provides the definitions and structures required for
 * managing BLE sessions, including system, DP (Data Point), channel, and user
 * sessions. It also includes the configuration structure for initializing the
 * BLE functionality with Tuya IoT SDK.
 *
 * The BLE Manager is responsible for handling the BLE communication setup,
 * session management, and the integration with Tuya's IoT platform, ensuring
 * secure and efficient communication between the device and the cloud.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_MGR_H__
#define __BLE_MGR_H__

#include "tuya_cloud_types.h"
#include "ble_protocol.h"
#include "ble_cryption.h"
#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    BLE_SESSION_SYSTEM,
    BLE_SESSION_DP,
    BLE_SESSION_CHANNEL,
    BLE_SESSION_USER,
    BLE_SESSION_MAX
} ble_seesion_type_t;

#define TUYA_BLE_NAME_LEN 5

typedef struct {
    tuya_iot_client_t *client;
    char device_name[TUYA_BLE_NAME_LEN + 1];
} tuya_ble_cfg_t;

typedef struct {
    uint32_t sn;
    uint16_t type;
    uint16_t len;
    uint8_t *data;
    uint8_t encrypt_mode;
} ble_packet_t;

typedef void (*ble_session_fn_t)(ble_packet_t *packet, void *priv_data);

/**
 * @brief Initializes the Tuya BLE module.
 *
 * This function initializes the Tuya BLE module with the provided
 * configuration.
 *
 * @param cfg Pointer to the Tuya BLE configuration structure.
 * @return 0 if successful, otherwise an error code.
 */
int tuya_ble_init(tuya_ble_cfg_t *cfg);

/**
 * @brief Deinitializes the Tuya BLE module.
 *
 * This function deinitializes the Tuya BLE module and releases resources.
 *
 * @return 0 if successful, otherwise an error code.
 */
int tuya_ble_deinit(void);

/**
 * @brief Checks if the device is connected to the Tuya BLE service.
 *
 * @return true if the device is connected, false otherwise.
 */
bool tuya_ble_is_connected(void);

/**
 * @brief Sends data over the BLE connection.
 *
 * This function is used to send data over the BLE connection established by the
 * Tuya Open SDK for devices.
 *
 * @param type The type of the data being sent.
 * @param ack_sn The acknowledgement sequence number.
 * @param data Pointer to the data buffer.
 * @param len The length of the data buffer.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tuya_ble_send(uint16_t type, uint32_t ack_sn, uint8_t *data, uint32_t len);

/**
 * @brief Sends a packet over BLE.
 *
 * This function is used to send a packet over BLE using the Tuya Open SDK for
 * devices.
 *
 * @param packet Pointer to the BLE packet to be sent.
 * @return Returns an integer value indicating the status of the packet send
 * operation. A return value of 0 indicates success, while a non-zero value
 * indicates an error.
 */
int tuya_ble_send_packet(ble_packet_t *packet);

/**
 * @brief Enables or disables debug log output for Tuya BLE.
 *
 * This function allows you to enable or disable debug log for Tuya BLE.
 *
 * @param enable Set to true to enable debug log, or false to disable it.
 */
void tuya_ble_enable_debug(bool enable);

/**
 * @brief Prints the raw data buffer.
 *
 * This function prints the raw data buffer with the specified title, width,
 * buffer, and size.
 *
 * @param title The title of the raw data.
 * @param width The width of the raw data.
 * @param buf The pointer to the raw data buffer.
 * @param size The size of the raw data buffer.
 */
void tuya_ble_raw_print(char *title, uint8_t width, uint8_t *buf, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif

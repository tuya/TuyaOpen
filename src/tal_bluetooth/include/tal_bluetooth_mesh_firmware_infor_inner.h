/**
 * @file tal_bluetooth_mesh_firmware_infor_inner.h
 * @brief Defines the internal structures and API functions for managing
 * firmware information in Tuya's Bluetooth Mesh devices.
 *
 * This header file contains the definition of the firmware information
 * structure, which includes details such as the firmware version, mesh
 * category, and whether a publish address is needed. It also declares functions
 * for accessing these firmware details, enabling the management and query of
 * firmware-related information within the Bluetooth Mesh network.
 *
 * The APIs and structures defined in this file are intended for internal use
 * within the Tuya Bluetooth Mesh SDK, providing foundational support for
 * firmware management and device functionality within the mesh network.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_BLUETOOTH_MESH_FIRMWARE_INFOR_INNER_H__
#define __TAL_BLUETOOTH_MESH_FIRMWARE_INFOR_INNER_H__

typedef struct {
    uint8_t is_key;        /**< Mesh Node Address */
    uint8_t pid_or_key[8]; /**< Mesh Data, structure refer to @TAL_BLE_DATA_T */
    uint32_t version;
    uint32_t mesh_category;
    uint8_t need_publish_addr;
} TAL_MESH_FIRMWARE_INFOR_T;

uint8_t tal_get_firmware_key_or_pid(uint8_t *pid_or_key);

uint32_t tal_get_firmware_version(void);

uint32_t tal_get_firmware_mesh_category(void);

uint8_t tal_get_firmware_if_need_publish_addr(void);

#endif

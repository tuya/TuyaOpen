/**
 * @file rfid_scan.h
 * @brief Implementation of RFID scan functions
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __RFID_SCAN_H__
#define __RFID_SCAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

typedef enum {
    RFID_DATA_TYPE_UID = 0x0000,
    RFID_DATA_TYPE_BLOCK_DATA = 0x0001,
} RFID_DATA_TYPE_E;

typedef enum {
    RFID_TAG_TYPE_UNKNOWN = 0x0000,
    RFID_TAG_TYPE_MIFARE_CLASSIC_4K = 0x0002,
    RFID_TAG_TYPE_MIFARE_CLASSIC_1K = 0x0004,
    RFID_TAG_TYPE_MIFARE_ULTRALIGHT = 0x0044,
    RFID_TAG_TYPE_TYPE_B            = 0x0900, // ID reading is limited to residents with CN (China) ID cards only
    RFID_TAG_TYPE_15693             = 0x3D4D,
} RFID_TAG_TYPE_E;

typedef enum {
    RFID_SCAN_LENGTH_4_BYTES = 0x0004,  // For UIDs less than 16 bytes, all the remaining bytes are 00h.
    RFID_SCAN_LENGTH_7_BYTES = 0x0007,
    RFID_SCAN_LENGTH_8_BYTES = 0x0008,
    RFID_SCAN_LENGTH_16_BYTES = 0x0010,
} RFID_SCAN_LENGTH_E;

typedef enum {
    RFID_DATA_CMD_READ = 0x17,
} RFID_DATA_CMD_E;

/**
 * @brief RFID tag information structure
 */
typedef struct {
    uint8_t dev_id;                 /**< Device ID (1 byte) */
    uint16_t tag_type;              /**< Tag Type (2 bytes) */
    uint8_t uid[16];                /**< UID (4-16 bytes) */
    uint8_t uid_length;             /**< Actual UID length */
    bool is_valid;                  /**< Whether tag data is valid */
} rfid_tag_info_t;

typedef struct {
    RFID_DATA_TYPE_E data_type;
    RFID_TAG_TYPE_E tag_type;
    uint16_t block_addr;    // only data_type is RFID_DATA_TYPE_BLOCK_DATA used
    RFID_SCAN_LENGTH_E data_len;
    uint8_t data[16];       // For UIDs less than 16 bytes, all the remaining bytes are 00h.
} RFID_SCAN_DATA;

typedef struct {
    uint8_t dev_id;
    RFID_DATA_CMD_E cmd;
    uint8_t length;
    RFID_SCAN_DATA data;
    uint16_t crc;
} RFID_SCAN_FRAME;

/**
 * @brief RFID scan callback function type
 * @param dev_id Device ID
 * @param tag_type Tag type
 * @param uid UID data buffer
 * @param uid_len UID length
 */
typedef void (*rfid_scan_callback_t)(uint8_t dev_id, RFID_TAG_TYPE_E tag_type, const uint8_t *uid, uint8_t uid_len);

/**
 * @brief Process RFID scan data from UART buffer
 * @param buffer UART read buffer
 * @param len Buffer length
 * @param callback Callback function when valid RFID data is detected
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET rfid_scan_process(const uint8_t *buffer, int len, rfid_scan_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __RFID_SCAN_H__ */
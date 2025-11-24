/**
 * @file rfid_scan_screen.h
 * @brief Declaration of the RFID scan screen for the application
 *
 * This file contains the declarations for the RFID scan screen which displays
 * RFID tag information including device ID, tag type, and UID.
 *
 * The RFID scan screen includes:
 * - Device ID display (1 byte)
 * - Tag Type display (2 bytes)
 * - UID display (4-16 bytes variable length)
 * - Visual card-style presentation
 * - Real-time scan status indicator
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef RFID_SCAN_SCREEN_H
#define RFID_SCAN_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

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

extern Screen_t rfid_scan_screen;

void rfid_scan_screen_init(void);
void rfid_scan_screen_deinit(void);

/**
 * @brief Update RFID tag information on screen
 * @param tag_info Pointer to RFID tag information structure
 */
void rfid_scan_screen_update_tag(const rfid_tag_info_t *tag_info);

/**
 * @brief Clear displayed RFID tag information
 */
void rfid_scan_screen_clear_tag(void);

/**
 * @brief Callback function to update RFID tag information from rfid_scan module
 * This function should be called by rfid_scan.c when new tag data is received
 * @param dev_id Device ID
 * @param tag_type Tag type
 * @param uid Pointer to UID data
 * @param uid_length Length of UID data
 */
void rfid_scan_screen_data_update(uint8_t dev_id, uint16_t tag_type, const uint8_t *uid, uint8_t uid_length);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*RFID_SCAN_SCREEN_H*/

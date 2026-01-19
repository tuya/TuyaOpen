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
#include "rfid_scan.h"

extern Screen_t rfid_scan_screen;

void rfid_scan_screen_init(void);
void rfid_scan_screen_deinit(void);

/**
 * @brief Callback function to update RFID tag information from rfid_scan module
 * This function should be called by rfid_scan.c when new tag data is received
 * @param dev_id Device ID
 * @param tag_type Tag type
 * @param uid Pointer to UID data
 * @param uid_length Length of UID data
 */
void rfid_scan_screen_data_update(uint8_t dev_id, uint16_t tag_type, const uint8_t *uid, uint8_t uid_length);

void rfid_scan_screen_load(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*RFID_SCAN_SCREEN_H*/
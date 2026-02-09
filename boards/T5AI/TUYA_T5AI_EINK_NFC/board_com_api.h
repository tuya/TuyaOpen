/**
 * @file board_com_api.h
 * @author Tuya Inc.
 * @brief Header file for common board-level hardware registration APIs.
 *
 * This board supports:
 * - E-Ink Display: UC8276 400x300 (SPI interface)
 * - Touch Panel: FT6336 (I2C interface)
 * - Audio, Buttons, LED, SD Card, Power Domains
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"
#include "board_power_domain_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// E-Ink Display (UC8276 400x300) pin assignments
// SPI Interface:
//   - SCLK: P44
//   - SDI (MOSI): P46
//   - CS: P45
//   - D/C: P47
//   - RST: P43
//   - Backlight: P33
//
// Touch Panel (FT6336) pin assignments
// I2C Interface:
//   - SDA: P21
//   - SCK: P20
//   - RST: P37
//   - INT: P36

/***********************************************************
********************function declaration********************
***********************************************************/
// Forward declarations for battery functions
// Full declarations are in board_charge_detect_api.h
OPERATE_RET board_battery_read(uint32_t *voltage_mv, uint8_t *percentage);

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
 * @brief Initialize SD card hardware (power and pinmux)
 *
 * This function ensures the SD card power domain is enabled and SDIO pins are configured.
 * After calling this, applications can mount the SD card using tkl_fs_mount().
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_sdcard_init(void);

/**
 * @brief Registers all the hardware peripherals (audio, button, LED, SD card, display, TP) on the board.
 *
 * This function initializes and registers:
 * - Power domains (EINK 3V3, SD card 3V3)
 * - Audio codec
 * - Buttons (UP, DOWN, ENTER, RETURN, LEFT, RIGHT)
 * - User LED
 * - E-Ink display (UC8276 400x300 via SPI)
 * - Touch panel (FT6336 via I2C)
 * - SDIO interface for SD card
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */

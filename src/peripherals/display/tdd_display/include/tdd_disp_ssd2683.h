/**
 * @file tdd_disp_ssd2683.h
 * @brief SSD2683 E-Ink display driver header file
 *
 * Register/command definitions and the registration API for the SSD2683 e-paper
 * controller (B/W, up to 400x300). This panel is driven over a 4-wire SPI bus that
 * is bit-banged in software (the controller and refresh flow follow the vendor
 * reference, see docs/4D2_BW_SSD2683_300x400_Code.c).
 *
 * BUSY polarity: HIGH = ready / idle (the controller pulls BUSY LOW while busy).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_SSD2683_H__
#define __TDD_DISP_SSD2683_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* SSD2683 Commands */
#define SSD2683_PANEL_SETTING   0x00
#define SSD2683_POWER_OFF       0x02
#define SSD2683_POWER_ON        0x04
#define SSD2683_DEEP_SLEEP      0x07
#define SSD2683_DATA_START_TX   0x10
#define SSD2683_DISPLAY_REFRESH 0x12
#define SSD2683_PARTIAL_WINDOW  0x83
#define SSD2683_TEMP_SENSOR_CMD 0x40
#define SSD2683_VCOM_SETTING    0x50
#define SSD2683_CASCADE_SETTING 0xE0
#define SSD2683_FORCE_TEMP      0xE6
#define SSD2683_BOOST_OTP       0xE9
#define SSD2683_ACT_TEMP        0xA5

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief SSD2683 E-Ink display software-SPI device configuration
 */
typedef struct {
    uint16_t                width;               /**< Display width in pixels */
    uint16_t                height;              /**< Display height in pixels */
    TUYA_DISPLAY_ROTATION_E rotation;            /**< Display rotation */
    TUYA_GPIO_NUM_E         clk_pin;             /**< Software-SPI clock (SCK) */
    TUYA_GPIO_NUM_E         sda_pin;             /**< Software-SPI data (MOSI/SDA, also used for reads) */
    TUYA_GPIO_NUM_E         cs_pin;              /**< Chip select (CS) */
    TUYA_GPIO_NUM_E         dc_pin;              /**< Data/Command (DC) */
    TUYA_GPIO_NUM_E         rst_pin;             /**< Reset (RST) */
    TUYA_GPIO_NUM_E         busy_pin;            /**< Busy status (set to TUYA_GPIO_NUM_MAX if unused) */
    TUYA_DISPLAY_IO_CTRL_T  power;               /**< Optional panel power control */
    uint16_t                full_refresh_period; /**< Force a full (de-ghost) refresh every N fast
                                                      refreshes; 0 = always full refresh */
} DISP_EINK_SSD2683_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an SSD2683 E-Ink mono display device (software SPI) with the
 *        display management system.
 *
 * @param name    Name of the display device (used for identification).
 * @param dev_cfg Pointer to the E-Ink device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_sw_spi_mono_ssd2683_register(char *name, DISP_EINK_SSD2683_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SSD2683_H__ */

/**
 * @file tdd_disp_uc8276.h
 * @brief UC8276 E-Ink display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the UC8276 E-Ink display controller. The UC8276 is a driver IC
 * for e-paper displays, supporting resolutions up to 400x300 with SPI interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_UC8276_H__
#define __TDD_DISP_UC8276_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* UC8276 Commands */
#define UC8276_PANEL_SETTING          0x00
#define UC8276_POWER_SETTING          0x01
#define UC8276_POWER_OFF              0x02
#define UC8276_POWER_OFF_SEQ          0x03
#define UC8276_POWER_ON               0x04
#define UC8276_POWER_ON_MEASURE       0x05
#define UC8276_BOOSTER_SOFT_START     0x06
#define UC8276_DEEP_SLEEP             0x07
#define UC8276_DATA_START_TRANS_1     0x10
#define UC8276_DATA_STOP              0x11
#define UC8276_DISPLAY_REFRESH        0x12
#define UC8276_DATA_START_TRANS_2     0x13
#define UC8276_DUAL_SPI_MODE          0x15
#define UC8276_AUTO_SEQ               0x17
#define UC8276_LUT_FOR_VCOM           0x20
#define UC8276_LUT_WHITE_TO_WHITE     0x21
#define UC8276_LUT_BLACK_TO_WHITE     0x22
#define UC8276_LUT_WHITE_TO_BLACK     0x23
#define UC8276_LUT_BLACK_TO_BLACK     0x24
#define UC8276_PLL_CONTROL            0x30
#define UC8276_TEMP_SENSOR_CMD        0x40
#define UC8276_TEMP_SENSOR_SELECTION  0x41
#define UC8276_TEMP_SENSOR_WRITE      0x42
#define UC8276_TEMP_SENSOR_READ       0x43
#define UC8276_PANEL_BREAK_CHECK      0x44
#define UC8276_VCOM_AND_DATA_SETTING  0x50
#define UC8276_TCON_SETTING           0x60
#define UC8276_RESOLUTION_SETTING     0x61
#define UC8276_GSST_SETTING           0x65
#define UC8276_GET_STATUS             0x71
#define UC8276_AUTO_MEASURE_VCOM      0x80
#define UC8276_READ_VCOM_VALUE        0x81
#define UC8276_VCOM_DC_SETTING        0x82
#define UC8276_PARTIAL_WINDOW         0x90
#define UC8276_PARTIAL_IN             0x91
#define UC8276_PARTIAL_OUT            0x92
#define UC8276_PROGRAM_MODE           0xA0
#define UC8276_ACTIVE_PROGRAM         0xA1
#define UC8276_READ_OTP               0xA2
#define UC8276_CASCADE_SETTING        0xE0
#define UC8276_POWER_SAVING           0xE3
#define UC8276_LVD_VOLTAGE_SELECT     0xE4
#define UC8276_FORCE_TEMP             0xE5
#define UC8276_TEMP_BOUNDARY_PHASE_C2 0xE7

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief UC8276 E-Ink display SPI device configuration
 */
typedef struct {
    uint16_t                width;    /**< Display width in pixels */
    uint16_t                height;   /**< Display height in pixels */
    TUYA_DISPLAY_ROTATION_E rotation; /**< Display rotation */
    TUYA_GPIO_NUM_E         cs_pin;   /**< SPI chip select pin */
    TUYA_GPIO_NUM_E         dc_pin;   /**< Data/Command pin */
    TUYA_GPIO_NUM_E         rst_pin;  /**< Reset pin */
    TUYA_GPIO_NUM_E         busy_pin; /**< Busy status pin (set to TUYA_GPIO_NUM_MAX if not used) */
    TUYA_SPI_NUM_E          port;     /**< SPI port number */
    uint32_t                spi_clk;  /**< SPI clock frequency */
    TUYA_DISPLAY_BL_CTRL_T  bl;       /**< Backlight control (for front light if available) */
    TUYA_DISPLAY_IO_CTRL_T  power;    /**< Power control configuration */
} DISP_EINK_UC8276_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a UC8276 E-Ink mono display device using SPI with the display management system.
 *
 * This function creates and initializes a new UC8276 E-Ink display device instance,
 * configures its frame buffer and hardware-specific settings, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the E-Ink device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_mono_uc8276_register(char *name, DISP_EINK_UC8276_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_UC8276_H__ */

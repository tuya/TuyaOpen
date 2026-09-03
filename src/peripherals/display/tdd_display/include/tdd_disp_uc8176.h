/**
 * @file tdd_disp_uc8176.h
 * @brief UC8176 (IL0398) E-Ink display driver header file
 *
 * The UC8176 is the controller on the first revision of the Waveshare 4.2 inch
 * e-paper module, 400x300 and one bit per pixel. It is not a variant of the
 * SSD1683 that tdd_disp_uc8276.c drives -- the two disagree about the command
 * set and about the sense of the BUSY line, so a panel on one controller is
 * simply silent under the other's driver.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_UC8176_H__
#define __TDD_DISP_UC8176_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* UC8176 commands */
#define UC8176_PANEL_SETTING      0x00
#define UC8176_POWER_SETTING      0x01
#define UC8176_POWER_OFF          0x02
#define UC8176_POWER_OFF_SEQ      0x03
#define UC8176_POWER_ON           0x04
#define UC8176_BOOSTER_SOFT_START 0x06
#define UC8176_DEEP_SLEEP         0x07
#define UC8176_DATA_START_TRANS_1 0x10 /* DTM1: the image being replaced */
#define UC8176_DATA_STOP          0x11
#define UC8176_DISPLAY_REFRESH    0x12
#define UC8176_DATA_START_TRANS_2 0x13 /* DTM2: the image being drawn */
#define UC8176_PLL_CONTROL        0x30
#define UC8176_VCOM_DATA_INTERVAL 0x50
#define UC8176_TCON_SETTING       0x60
#define UC8176_RESOLUTION_SETTING 0x61
#define UC8176_GET_STATUS         0x71
#define UC8176_VCOM_DC_SETTING    0x82

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief UC8176 E-Ink display SPI device configuration
 */
typedef struct {
    uint16_t                width;    /**< Display width in pixels */
    uint16_t                height;   /**< Display height in pixels */
    TUYA_DISPLAY_ROTATION_E rotation; /**< Display rotation */
    TUYA_GPIO_NUM_E         cs_pin;   /**< SPI chip select pin, TUYA_GPIO_NUM_MAX when the SPI block drives it */
    TUYA_GPIO_NUM_E         dc_pin;   /**< Data/Command pin */
    TUYA_GPIO_NUM_E         rst_pin;  /**< Reset pin */
    TUYA_GPIO_NUM_E         busy_pin; /**< Busy status pin (TUYA_GPIO_NUM_MAX if not wired) */
    TUYA_SPI_NUM_E          port;     /**< SPI port number */
    uint32_t                spi_clk;  /**< SPI clock frequency */
    TUYA_DISPLAY_BL_CTRL_T  bl;       /**< Backlight control (for a front light, if any) */
    TUYA_DISPLAY_IO_CTRL_T  power;    /**< Power control configuration */
} DISP_EINK_UC8176_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a UC8176 E-Ink mono display device using SPI with the display management system.
 *
 * This function creates and initializes a new UC8176 E-Ink display device instance,
 * configures its frame buffer and hardware-specific settings, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the E-Ink device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_mono_uc8176_register(char *name, DISP_EINK_UC8176_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_UC8176_H__ */

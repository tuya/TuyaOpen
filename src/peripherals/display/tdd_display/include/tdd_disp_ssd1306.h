/**
 * @file tdd_disp_ssd1306.h
 * @brief SSD1306 display driver interface definitions.
 *
 * This header provides macro definitions and function declarations for
 * controlling the SSD1306 OLED display via I2C interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_SSD1306_H__
#define __TDD_DISP_SSD1306_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define SSD1306_I2C_ADDR      0x3C // Default I2C address for SSD1306 OLED display
#define SSD1306_I2C_ADDR2     0x3D // Alternative I2C address for SSD1306 OLED display

#define SSD1306_CMD_REG       0x00 // Command register for SSD1306
#define SSD1306_DATA_REG      0x40 // Data register for SSD1306

#define SSD1306_COM_PIN_CMD   0xDA // Command for setting COM pins hardware configuration
#define SSD1306_COM_PIN_CFG   0x12 // Command for setting COM pins configuration
#define SSD1306_COM_PIN_CFG2  0x02 
#define SSD1306_COM_PIN_CFG3  0x22
#define SSD1306_COM_PIN_CFG4  0x32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool    is_color_inverse;
    uint8_t com_pin_cfg;
} DISP_SSD1306_INIT_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an SSD1306 OLED display device over I2C with the display management system.
 *
 * This function creates and initializes a new SSD1306 OLED display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the I2C OLED device configuration structure.
 * @param init_cfg Pointer to the SSD1306-specific initialization configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_i2c_oled_ssd1306_register(char *name, DISP_I2C_OLED_DEVICE_CFG_T *dev_cfg,\
                                               DISP_SSD1306_INIT_CFG_T *init_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SSD1306_H__ */

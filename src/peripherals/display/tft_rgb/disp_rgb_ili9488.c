/**
 * @file disp_rgb_ili9488.c
 * @brief ILI9488 LCD Display Driver Implementation
 * 
 * This file contains the implementation for driving the ILI9488 LCD display using software SPI.
 * It includes the initialization sequence, configuration settings, and functions to register
 * the LCD device with the display driver framework.
 * 
 * @details The file defines constants for the ILI9488 display configuration and initialization
 * sequence. It also provides a function to register the LCD device, which initializes the
 * display with the specified configuration and registers it with the display driver system.
 * 
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tkl_system.h"
#include "tkl_gpio.h"
#include "tal_log.h"

#include "lcd_ili9488.h"
#include "tkl_disp_drv_lcd.h"
#include "disp_sw_spi_driver.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************const define**********************
***********************************************************/
const TUYA_LCD_RGB_CFG_T cILI9488_CFG = {
    .clk = TUYA_LCD_15M,
    .active_edge = TUYA_RGB_NEGATIVE_EDGE,
    .hsync_pulse_width = 20,
    .vsync_pulse_width = 4,
    .hsync_back_porch  = 80,
    .hsync_front_porch = 80,
    .vsync_back_porch  = 8,
    .vsync_front_porch = 8,
};

const uint8_t cILI9488_INIT_SEQ[] = {
    3,  0,   ILI9488_PWCTR1,   0x0E, 0x0E,
    2,  0,   ILI9488_PWCTR2,   0x46,
    4,  0,   ILI9488_VMCTR1,   0x00, 0x2D, 0x80,
    2,  0,   ILI9488_IFMODE,   0x00,
    2,  0,   ILI9488_FRMCTR1,  0xA0,
    2,  0,   ILI9488_INVCTR,   0x02,
    5,  0,   ILI9488_PRCTR,    0x08, 0x0C, 0x50, 0x64,
    3,  0,   ILI9488_DFUNCTR,  0x32, 0x02,
    2,  0,   ILI9488_MADCTL,   0x48,
    2,  0,   ILI9488_PIXFMT,   0x70,
    2,  0,   ILI9488_INVON,    0x00,
    2,  0,   ILI9488_SETIMAGE, 0x01,
    5,  0,   ILI9488_ACTRL3,   0xA9, 0x51, 0x2C, 0x82,
    3,  0,   ILI9488_ACTRL4,   0x21, 0x05,
    16, 0,   ILI9488_GMCTRP1,  0x00, 0x0C, 0x10, 0x03, 0x0F, 0x05, 0x37, 0x66, 0x4D, 0x03, 0x0C, 0x0A, 0x2F, 0x35, 0x0F,
    16, 0,   ILI9488_GMCTRN1,  0x00, 0x0F, 0x16, 0x06, 0x13, 0x07, 0x3B, 0x35, 0x51, 0x07, 0x10, 0x0D, 0x36, 0x3B, 0x0F,
    1,  120, ILI9488_SLPOUT, 
    1,  20,  ILI9488_DISPON,
    0,
};
/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Register an LCD device.
 * 
 * This function initializes and registers an LCD device based on the provided device ID.
 * It configures the device's basic properties such as resolution, pixel format, rotation,
 * and backlight mode. Depending on the backlight mode, it sets up either GPIO or PWM parameters.
 * Finally, the function registers the LCD device using the configured parameters.
 * 
 * @param dev_id The unique identifier for the LCD device to be registered.
 * @return OPERATE_RET Returns 0 (OPRT_OK) on success, or a non-zero error code on failure.
 */
OPERATE_RET tuya_lcd_device_register(int dev_id)
{
    OPERATE_RET ret = 0;
    TUYA_LCD_CFG_T lcd_device;

    disp_sw_spi_lcd_init_seq(cILI9488_INIT_SEQ);

    memset(&lcd_device, 0x00, sizeof(TUYA_LCD_CFG_T));

    lcd_device.id       = dev_id;
    lcd_device.width    = DISPLAY_LCD_WIDTH;
    lcd_device.height   = DISPLAY_LCD_HEIGHT;
    lcd_device.fmt      = TKL_DISP_PIXEL_FMT_RGB565;
    lcd_device.rotation = TKL_DISP_ROTATION_0;

    lcd_device.lcd_tp  = TUYA_LCD_TYPE_RGB;
    lcd_device.p_rgb   = &cILI9488_CFG;

    lcd_device.bl.mode = DISPLAY_LCD_BL_MODE;

    #if (DISPLAY_LCD_BL_MODE == TUYA_DISP_BL_GPIO)
    lcd_device.bl.gpio.io        = DISPLAY_LCD_BL_PIN;
    lcd_device.bl.gpio.active_lv = DISPLAY_LCD_BL_POLARITY_LEVEL;
    #else 
    lcd_device.bl.pwm.id            = DISPLAY_LCD_BL_PWM_ID;
    lcd_device.bl.pwm.cfg.polarity  = DISPLAY_LCD_BL_POLARITY_LEVEL;
    lcd_device.bl.pwm.cfg.frequency = DISPLAY_LCD_BL_PWM_FREQ;
    #endif

    ret = tkl_disp_register_lcd_dev(&lcd_device);
    if(ret != OPRT_OK) {
        PR_ERR("tkl_disp_register_lcd_dev error:%d", ret);
        return ret;
    }

    return OPRT_OK;
}
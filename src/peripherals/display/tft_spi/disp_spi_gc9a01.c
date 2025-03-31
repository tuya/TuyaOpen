/**
 * @file disp_spi_gc9a01.c
 * @brief Implementation of GC9A01 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for GC9A01 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"

#include "lcd_gc9a01.h"
#include "tkl_disp_drv_lcd.h"
#include "tal_log.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cGC9A01_INIT_SEQ[] = {
    1,    0,    0xEF, 
    2,    0,    0xEB, 0x14, 
    1,    0,    0xFE, 
    1,    0,    0xEF, 
    2,    0,    0xEB, 0x14,
    2,    0,    0x84, 0x40, 
    2,    0,    0x85, 0xFF, 
    2,    0,    0x86, 0xFF, 
    2,    0,    0x87, 0xFF, 
    2,    0,    0x88, 0x0A, 
    2,    0,    0x89, 0x21, 
    2,    0,    0x8A, 0x00, 
    2,    0,    0x8B, 0x80, 
    2,    0,    0x8C, 0x01, 
    2,    0,    0x8D, 0x01, 
    2,    0,    0x8E, 0xFF, 
    2,    0,    0x8F, 0xFF,
    3,    0,    0xB6, 0x00, 0x00, 
    2,    0,    0x36, 0x48, 
    2,    0,    0x3A, 0x05, //
    5,    0,    0x90, 0x08, 0x08, 0x08, 0x08,
    2,    0,    0xBD, 0x06,
    2,    0,    0xBC, 0x00, 
    4,    0,    0xFF, 0x60, 0x01, 0x04, 
    4,    0,    0xC3, 0x13, 0xC4, 0x13, 
    2,    0,    0xC9, 0x22, 
    2,    0,    0xBE, 0x11, 
    3,    0,    0xE1, 0x10, 0x0E, 
    4,    0,    0xDF, 0x31, 0x0c, 0x02,
    7,    0,    0xF0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A, 
    7,    0,    0xF1, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F, 
    7,    0,    0xF2, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A, 
    7,    0,    0xF3, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F, 
    3,    0,    0xED, 0x1B, 0x0B,
    2,    0,    0xAE, 0x77,
    2,    0,    0xCD, 0x63,
    10,   0,    0x70, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
    2,    0,    0xE8, 0x34,
    13,   0,    0x62, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
    13,   0,    0x63, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
    8,    0,    0x64, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
    11,   0,    0x66, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00, 
    11,   0,    0x67, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
    8,    0,    0x74, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
    3,    0,    0x98, 0x3e, 0x07,
    1,    0,    0x35, 1,    0,    0x21,
    1,    120,  0x11, 1,    20,   0x29,
    0 // Terminate list
};

const TUYA_LCD_SPI_CFG_T cGC9A01_CFG = {
    .rst_pin   = LCD_SPI_RST_PIN,
    .cs_pin    = LCD_SPI_CS_PIN,
    .dc_pin    = LCD_SPI_DC_PIN,
    .spi_port  = LCD_SPI_PORT,
    .spi_clk   = LCD_SPI_CLK,

    .cmd_caset = GC9A01_CASET,
    .cmd_raset = GC9A01_RASET,
    .cmd_ramwr = GC9A01_RAMWR,

    .init_seq  = cGC9A01_INIT_SEQ,
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

    memset(&lcd_device, 0x00, sizeof(TUYA_LCD_CFG_T));

    lcd_device.id       = dev_id;
    lcd_device.width    = DISPLAY_LCD_WIDTH;
    lcd_device.height   = DISPLAY_LCD_HEIGHT;
    lcd_device.fmt      = TKL_DISP_PIXEL_FMT_RGB565;
    lcd_device.rotation = TKL_DISP_ROTATION_0;

    lcd_device.lcd_tp  = TUYA_LCD_TYPE_SPI;
    lcd_device.p_spi   = &cGC9A01_CFG;

#if defined(ENABLE_LCD_POWER_CTRL) && (ENABLE_LCD_POWER_CTRL == 1)
    lcd_device.power_io        = DISPLAY_LCD_POWER_PIN;
    lcd_device.power_active_lv = DISPLAY_LCD_POWER_POLARITY_LEVEL;
#else
    lcd_device.power_io = INVALID_GPIO_PIN;
#endif

#if defined(ENABLE_LCD_BL_MODE_GPIO) && (ENABLE_LCD_BL_MODE_GPIO == 1)
    lcd_device.bl.mode           = TUYA_DISP_BL_GPIO;
    lcd_device.bl.gpio.io        = DISPLAY_LCD_BL_PIN;
    lcd_device.bl.gpio.active_lv = DISPLAY_LCD_BL_POLARITY_LEVEL;
#elif defined(ENABLE_LCD_BL_MODE_PWM) && (ENABLE_LCD_BL_MODE_PWM == 1)
    lcd_device.bl.mode              = TUYA_DISP_BL_PWM;
    lcd_device.bl.pwm.id            = DISPLAY_LCD_BL_PWM_ID;
    lcd_device.bl.pwm.cfg.polarity  = DISPLAY_LCD_BL_POLARITY_LEVEL;
    lcd_device.bl.pwm.cfg.frequency = DISPLAY_LCD_BL_PWM_FREQ;
#else 
    lcd_device.bl.mode = TUYA_DISP_BL_NOT_EXIST;
#endif
    
    ret = tkl_disp_register_lcd_dev(&lcd_device);
    if(ret != OPRT_OK) {
        bk_printf("tkl_disp_register_lcd_dev error:%d", ret);
        return ret;
    }

    return OPRT_OK;
}
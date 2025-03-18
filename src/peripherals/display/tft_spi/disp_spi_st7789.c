/**
 * @file disp_spi_st7789.c
 * @brief Implementation of ST7789 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for ST7789 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"

#include "lcd_st7789.h"
#include "tkl_disp_drv_lcd.h"
#include "tal_log.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7789_INIT_SEQ[] = {
    1,    100,  ST7789_SWRESET,                                 // Software reset
    1,    50,   ST7789_SLPOUT,                                  // Exit sleep mode
    2,    10,   ST7789_COLMOD,    0x55,                         // Set colour mode to 16 bit
    2,    0,    ST7789_VCMOFSET,  0x1a,                         // VCOM
    6,    0,    ST7789_PORCTRL,   0x05, 0x05, 0x00, 0x33, 0x33, // Porch Setting
    2,    0,    ST7789_GCTRL,     0x05,                         // Gate Control
    2,    0,    ST7789_VCOMS,     0x3f,                         // VCOMS setting
    2,    0,    ST7789_LCMCTRL,   0x2c,                         // LCM control
    2,    0,    ST7789_VDVVRHEN,  0x01,                         // VDV and VRH command enable
    2,    0,    ST7789_VRHS,      0x0f,                         // VRH set
    2,    0,    ST7789_VDVSET,    0x20,                         // VDV setting
    2,    0,    ST7789_FRCTR2,    0x01,                         // FR Control 2
    3,    0,    ST7789_PWCTRL1,   0xa4, 0xa1,                   // Power control 1
    2,    0,    ST7789_PWCTRL2,   0x03,                         // Power control 2
    4,    0,    ST7789_EQCTRL,    0x09, 0x09, 0x08,             // Equalize time control

    2,    0,    ST7789_MADCTL,    0x00, // Set MADCTL: row then column, refresh is bottom to top
    15,   0,    ST7789_PVGAMCTRL, 0xd0, 0x05, 0x09, 0x09, 0x08, 0x14, 0x28, 0x33, 0x3f, 0x07, 0x13, 0x14, 0x28, 0x30, // Positive voltage gamma control
    15,   0,    ST7789_NVGAMCTRL, 0xd0, 0x05, 0x09, 0x09, 0x08, 0x03, 0x24, 0x32, 0x32, 0x3b, 0x14, 0x13, 0x28, 0x2f, // Negative voltage gamma control
    1,    10,   ST7789_NORON,  // Normal display on, then 10 ms delay
    1,    10,   ST7789_DISPON, // Main screen turn on, then wait 500 ms
    0                          // Terminate list
};

const TUYA_LCD_SPI_CFG_T cST7789_CFG = {
    .rst_pin   = LCD_SPI_RST_PIN,
    .cs_pin    = LCD_SPI_CS_PIN,
    .dc_pin    = LCD_SPI_DC_PIN,
    .spi_port  = LCD_SPI_PORT,
    .spi_clk   = LCD_SPI_CLK,

    .cmd_caset = ST7789_CASET,
    .cmd_raset = ST7789_RASET,
    .cmd_ramwr = ST7789_RAMWR,

    .init_seq  = cST7789_INIT_SEQ,
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
    lcd_device.p_spi   = &cST7789_CFG;

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
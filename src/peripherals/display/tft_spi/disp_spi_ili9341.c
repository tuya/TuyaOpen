/**
 * @file disp_spi_ili9341.c
 * @brief Implementation of ILI9341 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for ILI9341 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"

#include "lcd_ili9341.h"
#include "tkl_disp_drv_lcd.h"
#include "tal_log.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cILI9341_INIT_SEQ[] = {
    1, 100, ILI9341_SWRESET,              // Software reset
    1, 50,  ILI9341_SLPOUT,               // Exit sleep mode
    3, 0,   ILI9341_DSIPCTRL, 0x0a, 0xc2, // Display Function Control
    2, 0,   ILI9341_COLMOD,   0x55,       // Set colour mode to 16 bit
    2, 0,   ILI9341_MADCTL,   0x08,       // Set MADCTL: row then column, refresh is bottom to top
    1, 10,  ILI9341_DISPON,               // Main screen turn on, then wait 500 ms
    0                                     // Terminate list
};

const TUYA_LCD_SPI_CFG_T cILI9341_CFG = {
    .rst_pin   = LCD_SPI_RST_PIN,
    .cs_pin    = LCD_SPI_CS_PIN,
    .dc_pin    = LCD_SPI_DC_PIN,
    .spi_port  = LCD_SPI_PORT,
    .spi_clk   = LCD_SPI_CLK,

    .cmd_caset = ILI9341_CASET,
    .cmd_raset = ILI9341_RASET,
    .cmd_ramwr = ILI9341_RAMWR,

    .init_seq  = cILI9341_INIT_SEQ,
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
    lcd_device.p_spi   = &cILI9341_CFG;

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
        PR_ERR("tkl_disp_register_lcd_dev error:%d", ret);
        return ret;
    }

    return OPRT_OK;
}
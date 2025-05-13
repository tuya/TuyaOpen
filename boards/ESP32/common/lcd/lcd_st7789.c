/**
 * @file lcd_st7789.c
 * @brief lcd_st7789 module is used to
 * @version 0.1
 * @date 2025-05-13
 */

#include "lcd_st7789.h"

#include "board_config.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#include <driver/gpio.h>

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
} LCD_CONFIG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static LCD_CONFIG_T lcd_config = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

int lcd_st7789_init(void)
{
    return 0;
}

void *lcd_st7789_get_panel_io_handle(void)
{
    return lcd_config.panel_io;
}

void *lcd_st7789_get_panel_handle(void)
{
    return lcd_config.panel;
}

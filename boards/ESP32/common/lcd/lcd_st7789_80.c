/**
 * @file lcd_st7789.c
 * @brief lcd_st7789 module is used to
 * @version 0.1
 * @date 2025-05-13
 */
#include "board_config.h"

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_ST7789_80)

#include "lcd_st7789_80.h"

#include <driver/gpio.h>

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
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

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

int lcd_st7789_80_init(void)
{
    gpio_config_t gpio_init_struct;
    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;
    gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;
    gpio_init_struct.pin_bit_mask = 1ull << LCD_I80_RD;
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&gpio_init_struct);
    gpio_set_level(LCD_I80_RD, 1);

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = LCD_I80_DC,
        .wr_gpio_num = LCD_I80_WR,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums =
            {
                LCD_I80_D0,
                LCD_I80_D1,
                LCD_I80_D2,
                LCD_I80_D3,
                LCD_I80_D4,
                LCD_I80_D5,
                LCD_I80_D6,
                LCD_I80_D7,
            },
        .bus_width = 8,
        .max_transfer_bytes = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_I80_CS,
        .pclk_hz = (10 * 1000 * 1000),
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels =
            {
                .dc_idle_level = 0,
                .dc_cmd_level = 0,
                .dc_dummy_level = 0,
                .dc_data_level = 1,
            },
        .flags =
            {
                .swap_color_bytes = 0,
            },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &lcd_config.panel_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_I80_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_config.panel_io, &panel_config, &lcd_config.panel));

    esp_lcd_panel_reset(lcd_config.panel);
    esp_lcd_panel_init(lcd_config.panel);
    esp_lcd_panel_invert_color(lcd_config.panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    esp_lcd_panel_set_gap(lcd_config.panel, 0, 0);
    uint8_t data0[] = {0x00};
    uint8_t data1[] = {0x65};
    esp_lcd_panel_io_tx_param(lcd_config.panel_io, 0x36, data0, 1);
    esp_lcd_panel_io_tx_param(lcd_config.panel_io, 0x3A, data1, 1);
    esp_lcd_panel_swap_xy(lcd_config.panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(lcd_config.panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    return 0;
}

void *lcd_st7789_80_get_panel_io_handle(void)
{
    return lcd_config.panel_io;
}

void *lcd_st7789_80_get_panel_handle(void)
{
    return lcd_config.panel;
}

#endif
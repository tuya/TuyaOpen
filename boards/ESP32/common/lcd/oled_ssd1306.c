/**
 * @file oled_ssd1306.c
 * @brief oled_ssd1306 module is used to
 * @version 0.1
 * @date 2025-05-12
 */

#include "oled_ssd1306.h"

#include "board_config.h"

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_OLED_SSD1306)

#include "esp_err.h"
#include "esp_log.h"

#include <driver/i2c_master.h>
#include "driver/gpio.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "oled_ssd1306"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
} OLED_CONFIG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static OLED_CONFIG_T lcd_config = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

int oled_ssd1306_init(void)
{
    // Initialize I2C and SSD1306 here
    i2c_master_bus_config_t bus_config = {
        .i2c_port = (i2c_port_t)OLED_I2C_PORT,
        .sda_io_num = OLED_I2C_SDA,
        .scl_io_num = OLED_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &lcd_config.i2c_bus));
    ESP_LOGI(TAG, "I2C initialize successfully");

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_I2C_ADDR,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags =
            {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        .scl_speed_hz = 400 * 1000,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(lcd_config.i2c_bus, &io_config, &lcd_config.panel_io));
    ESP_LOGI(TAG, "I2C panel initialize successfully");

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.bits_per_pixel = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = OLED_HEIGHT,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(lcd_config.panel_io, &panel_config, &lcd_config.panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_config.panel));

    return 0;
}

void *oled_ssd1306_get_panel_io_handle(void)
{
    return lcd_config.panel_io;
}

void *oled_ssd1306_get_panel_handle(void)
{
    return lcd_config.panel;
}

#endif // BOARD_DISPLAY_TYPE == DISPLAY_TYPE_OLED_SSD1306

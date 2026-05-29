/**
 * @file tdd_disp_esp_ssd1306.c
 * @brief SSD1306 OLED register helper.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_ssd1306.h"

#include "esp_err.h"
#include "esp_log.h"

#include <driver/i2c_master.h>
#include "driver/gpio.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "tdd_disp_esp_ssd1306"

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    i2c_master_bus_handle_t   i2c_bus;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t    panel;
} OLED_CONFIG_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static OLED_CONFIG_T sg_lcd_config = {0};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Bring up the I2C bus + SSD1306 panel.
 * @param[in] hw Hardware configuration.
 * @return 0 on success.
 */
static int __oled_ssd1306_init(OLED_SSD1306_HW_CFG_T *hw)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port          = (i2c_port_t)hw->i2c_port,
        .sda_io_num        = hw->sda_io,
        .scl_io_num        = hw->scl_io,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &sg_lcd_config.i2c_bus));
    ESP_LOGI(TAG, "I2C initialize successfully");

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr            = hw->dev_addr,
        .on_color_trans_done = NULL,
        .user_ctx            = NULL,
        .control_phase_bytes = 1,
        .dc_bit_offset       = 6,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .flags =
            {
                .dc_low_on_data        = 0,
                .disable_control_phase = 0,
            },
        .scl_speed_hz = 400 * 1000,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(sg_lcd_config.i2c_bus, &io_config, &sg_lcd_config.panel_io));
    ESP_LOGI(TAG, "I2C panel initialize successfully");

    esp_lcd_panel_dev_config_t panel_config = {0};
    panel_config.reset_gpio_num             = -1;
    panel_config.bits_per_pixel             = 1;

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = hw->panel_height,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(sg_lcd_config.panel_io, &panel_config, &sg_lcd_config.panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(sg_lcd_config.panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(sg_lcd_config.panel));

    return 0;
}

OPERATE_RET tdd_disp_esp_ssd1306_register(char *name, OLED_SSD1306_HW_CFG_T *hw, TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (__oled_ssd1306_init(hw) != 0) {
        return OPRT_COM_ERROR;
    }

    return tdd_disp_esp_lcd_register(name, sg_lcd_config.panel_io, sg_lcd_config.panel, cfg);
}

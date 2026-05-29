/**
 * @file tdd_disp_esp_st7789_80.c
 * @brief ST7789 i80-bus LCD register helper.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_st7789_80.h"

#include <driver/gpio.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "tdd_disp_esp_st7789_80"

#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t    panel;
} LCD_CONFIG_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static LCD_CONFIG_T sg_lcd_config = {0};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Bring up the i80 bus + ST7789 panel.
 * @param[in] hw     Hardware configuration.
 * @param[in] width  Display width.
 * @param[in] height Display height.
 * @return 0 on success, -1 on failure.
 */
static int __lcd_st7789_80_init(LCD_ST7789_80_HW_CFG_T *hw, uint16_t width, uint16_t height)
{
    /* Tie LCD_RD high since most ST7789 i80 wirings are write-only. */
    gpio_config_t gpio_init_struct;
    gpio_init_struct.intr_type    = GPIO_INTR_DISABLE;
    gpio_init_struct.mode         = GPIO_MODE_INPUT_OUTPUT;
    gpio_init_struct.pin_bit_mask = 1ull << hw->rd_io;
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_init_struct.pull_up_en   = GPIO_PULLUP_ENABLE;
    gpio_config(&gpio_init_struct);
    gpio_set_level(hw->rd_io, 1);

    esp_lcd_i80_bus_handle_t i80_bus    = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = hw->dc_io,
        .wr_gpio_num = hw->wr_io,
        .clk_src     = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums =
            {
                hw->data_io[0],
                hw->data_io[1],
                hw->data_io[2],
                hw->data_io[3],
                hw->data_io[4],
                hw->data_io[5],
                hw->data_io[6],
                hw->data_io[7],
            },
        .bus_width          = 8,
        .max_transfer_bytes = (size_t)width * (size_t)height * sizeof(uint16_t),
        .psram_trans_align  = 64,
        .sram_trans_align   = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num         = hw->cs_io,
        .pclk_hz             = (10 * 1000 * 1000),
        .trans_queue_depth   = 10,
        .on_color_trans_done = NULL,
        .user_ctx            = NULL,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .dc_levels =
            {
                .dc_idle_level  = 0,
                .dc_cmd_level   = 0,
                .dc_dummy_level = 0,
                .dc_data_level  = 1,
            },
        .flags =
            {
                .swap_color_bytes = 0,
            },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &sg_lcd_config.panel_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = hw->rst_io,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(sg_lcd_config.panel_io, &panel_config, &sg_lcd_config.panel));

    esp_lcd_panel_reset(sg_lcd_config.panel);
    esp_lcd_panel_init(sg_lcd_config.panel);
    esp_lcd_panel_invert_color(sg_lcd_config.panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    esp_lcd_panel_set_gap(sg_lcd_config.panel, 0, 0);
    uint8_t data0[] = {0x00};
    uint8_t data1[] = {0x65};
    esp_lcd_panel_io_tx_param(sg_lcd_config.panel_io, 0x36, data0, 1);
    esp_lcd_panel_io_tx_param(sg_lcd_config.panel_io, 0x3A, data1, 1);
    esp_lcd_panel_swap_xy(sg_lcd_config.panel, hw->swap_xy);
    esp_lcd_panel_mirror(sg_lcd_config.panel, hw->mirror_x, hw->mirror_y);

    return 0;
}

OPERATE_RET tdd_disp_esp_st7789_80_register(char *name, LCD_ST7789_80_HW_CFG_T *hw, TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (__lcd_st7789_80_init(hw, cfg->width, cfg->height) != 0) {
        return OPRT_COM_ERROR;
    }

    return tdd_disp_esp_lcd_register(name, sg_lcd_config.panel_io, sg_lcd_config.panel, cfg);
}

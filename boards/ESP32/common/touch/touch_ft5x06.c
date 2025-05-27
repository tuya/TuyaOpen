/**
 * @file touch_ft5x06.c
 * @brief touch_ft5x06 module is used to
 * @version 0.1
 * @date 2025-05-22
 */
#include "board_config.h"

#if defined(BOARD_TOUCH_TYPE) && (BOARD_TOUCH_TYPE == TOUCH_TYPE_FT5X06)

#include "touch_ft5x06.h"

#include "esp_err.h"
#include "esp_log.h"

#include "sdkconfig.h"

#include "driver/i2c_master.h"

#include "esp_lcd_touch_ft5x06.h"
#include <esp_lvgl_port.h>
#include <lvgl.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "touch_ft5x06"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static esp_lcd_touch_handle_t tp_ft5x06 = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static i2c_master_bus_handle_t __i2c_init(int i2c_num, int scl_io, int sda_io)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t esp_rt = ESP_OK;

    // retrieve i2c bus handle
    esp_rt = i2c_master_get_bus_handle(i2c_num, &i2c_bus);
    if (esp_rt == ESP_OK && i2c_bus) {
        ESP_LOGI(TAG, "I2C bus handle retrieved successfully");
        return i2c_bus;
    }

    // initialize i2c bus
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = i2c_num,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    esp_rt = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(esp_rt));
        return NULL;
    }

    ESP_LOGI(TAG, "I2C bus initialized successfully");

    return i2c_bus;
}

int touch_ft5x06_init(void)
{
    i2c_master_bus_handle_t i2c_bus = __i2c_init(I2C_NUM, I2C_SCL_IO, I2C_SDA_IO);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return -1;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_WIDTH,
        .y_max = DISPLAY_HEIGHT,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_21,
        .levels =
            {
                .reset = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = 400 * 1000;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));
    ESP_LOGI(TAG, "Initialize touch controller");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp_ft5x06));

    return 0;
}

void *touch_ft5x06_get_handle(void)
{
    return tp_ft5x06;
}

#endif /* defined(BOARD_TOUCH_TYPE) && (BOARD_TOUCH_TYPE == TOUCH_TYPE_FT5X06) */
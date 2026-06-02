/**
 * @file tdd_tp_esp_ft5x06.c
 * @brief One-shot init + register helper for FT5x06 capacitive touch panels via ESP-IDF esp_lcd_touch.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_tp_esp_ft5x06.h"

#include "esp_err.h"
#include "esp_log.h"

#include "sdkconfig.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "tdd_tp_esp_ft5x06"

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Get or create an I2C master bus handle on the requested port.
 * @param[in] i2c_num I2C peripheral number.
 * @param[in] scl_io  SCL gpio number.
 * @param[in] sda_io  SDA gpio number.
 * @return Valid bus handle on success, NULL on failure.
 */
static i2c_master_bus_handle_t __i2c_init(int i2c_num, int scl_io, int sda_io)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t               esp_rt  = ESP_OK;

    esp_rt = i2c_master_get_bus_handle(i2c_num, &i2c_bus);
    if (esp_rt == ESP_OK && i2c_bus) {
        ESP_LOGI(TAG, "I2C bus handle retrieved successfully");
        return i2c_bus;
    }

    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port          = i2c_num,
        .sda_io_num        = sda_io,
        .scl_io_num        = scl_io,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
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

/**
 * @brief Initialise an FT5x06 touch controller and register it as a TuyaOpen TDD tp device.
 * @param[in] name Device name used for later lookup.
 * @param[in] cfg  I2C / GPIO / TP configuration.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tdd_tp_esp_i2c_ft5x06_register(char *name, TDD_TP_ESP_FT5X06_CFG_T *cfg)
{
    i2c_master_bus_handle_t       i2c_bus      = NULL;
    esp_lcd_panel_io_handle_t     tp_io_handle = NULL;
    esp_lcd_touch_handle_t        esp_tp_hdl   = NULL;
    esp_err_t                     esp_rt       = ESP_OK;

    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    i2c_bus = __i2c_init(cfg->i2c_port, cfg->i2c_scl_io, cfg->i2c_sda_io);
    if (NULL == i2c_bus) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return OPRT_COM_ERROR;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max        = cfg->tp.tp_cfg.x_max,
        .y_max        = cfg->tp.tp_cfg.y_max,
        .rst_gpio_num = (cfg->rst_io < 0) ? GPIO_NUM_NC : (gpio_num_t)cfg->rst_io,
        .int_gpio_num = (cfg->int_io < 0) ? GPIO_NUM_NC : (gpio_num_t)cfg->int_io,
        .levels =
            {
                .reset     = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy  = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
    };

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz                  = 400 * 1000;

    esp_rt = esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel io: %s", esp_err_to_name(esp_rt));
        return OPRT_COM_ERROR;
    }

    ESP_LOGI(TAG, "Initialize ft5x06 touch controller");
    esp_rt = esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &esp_tp_hdl);
    if (esp_rt != ESP_OK || NULL == esp_tp_hdl) {
        ESP_LOGE(TAG, "Failed to init ft5x06: %s", esp_err_to_name(esp_rt));
        return OPRT_COM_ERROR;
    }

    return tdd_tp_esp_register(name, (void *)esp_tp_hdl, &cfg->tp);
}

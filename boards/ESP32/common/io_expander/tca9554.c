/**
 * @file tca9554.c
 * @brief TCA9554 I2C IO expander helper (implementation).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tca9554.h"

#include "esp_err.h"
#include "esp_log.h"

#include "driver/i2c_master.h"
#include "esp_io_expander_tca9554.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "TCA9554"

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    i2c_master_bus_handle_t  i2c_bus;
    esp_io_expander_handle_t io_expander;
} TCA9554_CONFIG_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static TCA9554_CONFIG_T sg_tca9554_config = {0};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Get or create an I2C master bus handle on the requested port.
 * @param[in] i2c_num I2C peripheral number.
 * @param[in] scl_io  SCL gpio.
 * @param[in] sda_io  SDA gpio.
 * @return Bus handle or NULL on failure.
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

    return i2c_bus;
}

int tca9554_init(TCA9554_HW_CFG_T *hw)
{
    esp_err_t esp_rt = ESP_OK;

    if (sg_tca9554_config.io_expander) {
        ESP_LOGI(TAG, "TCA9554 I2C expander already initialized");
        return 0;
    }

    if (NULL == hw) {
        ESP_LOGE(TAG, "TCA9554 hw cfg is NULL");
        return -1;
    }

    sg_tca9554_config.i2c_bus = __i2c_init(hw->i2c_port, hw->scl_io, hw->sda_io);
    if (sg_tca9554_config.i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return -1;
    }

    esp_rt = esp_io_expander_new_i2c_tca9554(sg_tca9554_config.i2c_bus, hw->dev_addr,
                                             &sg_tca9554_config.io_expander);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TCA9554 I2C expander: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    ESP_LOGI(TAG, "TCA9554 I2C expander initialized successfully");

    return 0;
}

int tca9554_set_dir(uint32_t pin_num_mask, int is_input)
{
    esp_err_t esp_rt = ESP_OK;

    if (NULL == sg_tca9554_config.io_expander) {
        ESP_LOGE(TAG, "TCA9554 I2C expander not initialized");
        return -1;
    }

    if (is_input) {
        esp_rt = esp_io_expander_set_dir(sg_tca9554_config.io_expander, pin_num_mask, IO_EXPANDER_INPUT);
    } else {
        esp_rt = esp_io_expander_set_dir(sg_tca9554_config.io_expander, pin_num_mask, IO_EXPANDER_OUTPUT);
    }
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pin direction: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    return 0;
}

int tca9554_set_level(uint32_t pin_num_mask, int level)
{
    esp_err_t esp_rt = ESP_OK;

    if (NULL == sg_tca9554_config.io_expander) {
        ESP_LOGE(TAG, "TCA9554 I2C expander not initialized");
        return -1;
    }

    esp_rt = esp_io_expander_set_level(sg_tca9554_config.io_expander, pin_num_mask, level);

    return esp_rt;
}

int tca9554_get_level(uint32_t pin_num_mask, uint32_t *level)
{
    if (NULL == sg_tca9554_config.io_expander) {
        ESP_LOGE(TAG, "TCA9554 I2C expander not initialized");
        return -1;
    }

    if (NULL == level) {
        return -1;
    }

    esp_err_t esp_rt = esp_io_expander_get_level(sg_tca9554_config.io_expander, pin_num_mask, level);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get level: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    return 0;
}

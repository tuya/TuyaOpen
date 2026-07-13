/**
 * @file xl9555.c
 * @brief XL9555 16-bit I2C IO expander helper (implementation).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "xl9555.h"

#include "esp_err.h"
#include "esp_log.h"

#include "driver/i2c_master.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "XL9555"

/* xl9555 input register address */
#define XL9555_INPUT_PORT_0_REG_ADDR              (0x00)
#define XL9555_INPUT_PORT_1_REG_ADDR              (0x01)
/* xl9555 output register address */
#define XL9555_OUTPUT_PORT_0_REG_ADDR             (0x02)
#define XL9555_OUTPUT_PORT_1_REG_ADDR             (0x03)
/* xl9555 polarity inversion register address */
#define XL9555_POLARITY_INVERSION_PORT_0_REG_ADDR (0x04)
#define XL9555_POLARITY_INVERSION_PORT_1_REG_ADDR (0x05)
/* xl9555 configuration register address */
#define XL9555_CONFIGURATION_PORT_0_REG_ADDR      (0x06)
#define XL9555_CONFIGURATION_PORT_1_REG_ADDR      (0x07)

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t xl9555_handle;
} XL9555_CONFIG_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static XL9555_CONFIG_T sg_xl9555_config = {0};

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

int xl9555_init(XL9555_HW_CFG_T *hw)
{
    if (sg_xl9555_config.xl9555_handle) {
        ESP_LOGI(TAG, "XL9555 I2C expander already initialized");
        return 0;
    }

    if (NULL == hw) {
        ESP_LOGE(TAG, "XL9555 hw cfg is NULL");
        return -1;
    }

    sg_xl9555_config.i2c_bus = __i2c_init(hw->i2c_port, hw->scl_io, hw->sda_io);
    if (!sg_xl9555_config.i2c_bus) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return -1;
    }

    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = hw->dev_addr,
        .scl_speed_hz    = 400 * 1000,
        .scl_wait_us     = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(sg_xl9555_config.i2c_bus, &i2c_device_cfg,
                                              &sg_xl9555_config.xl9555_handle));
    if (NULL == sg_xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "Failed to create XL9555 I2C expander");
        return -1;
    }
    ESP_LOGI(TAG, "XL9555 I2C expander initialized successfully");

    return 0;
}

int xl9555_set_dir(uint32_t pin_num_mask, int is_input)
{
    esp_err_t esp_rt = ESP_OK;
    uint8_t   reg, value;

    if (NULL == sg_xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "XL9555 I2C expander not initialized");
        return -1;
    }

    if (pin_num_mask & 0x000000FF) {
        reg = XL9555_CONFIGURATION_PORT_0_REG_ADDR;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(sg_xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read configuration port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }

        if (is_input) {
            value = read_value | (pin_num_mask & 0x000000FF);
        } else {
            value = read_value & ~(pin_num_mask & 0x000000FF);
        }

        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(sg_xl9555_config.xl9555_handle, buffer, 2, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set configuration port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }
    }

    if (pin_num_mask & 0x0000FF00) {
        reg = XL9555_CONFIGURATION_PORT_1_REG_ADDR;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(sg_xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read configuration port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }

        if (is_input) {
            value = read_value | ((pin_num_mask >> 8) & 0xFF);
        } else {
            value = read_value & ~((pin_num_mask >> 8) & 0xFF);
        }

        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(sg_xl9555_config.xl9555_handle, buffer, 2, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set configuration port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }
    }

    return 0;
}

int xl9555_set_level(uint32_t pin_num_mask, uint32_t level)
{
    esp_err_t esp_rt = ESP_OK;

    if (NULL == sg_xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "XL9555 I2C expander not initialized");
        return -1;
    }

    if (pin_num_mask & 0x000000FF) {
        uint8_t reg   = XL9555_OUTPUT_PORT_0_REG_ADDR;
        uint8_t value = 0;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(sg_xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read output port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }

        if (level) {
            value = read_value | (pin_num_mask & 0x000000FF);
        } else {
            value = read_value & (~(pin_num_mask & 0x000000FF));
        }

        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(sg_xl9555_config.xl9555_handle, buffer, 2, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set output port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }
    }

    if (pin_num_mask & 0x0000FF00) {
        uint8_t reg   = XL9555_OUTPUT_PORT_1_REG_ADDR;
        uint8_t value = 0;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(sg_xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read output port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }

        if (level) {
            value = read_value | ((pin_num_mask >> 8) & 0xFF);
        } else {
            value = read_value & (~((pin_num_mask >> 8) & 0xFF));
        }

        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(sg_xl9555_config.xl9555_handle, buffer, 2, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set output port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }
    }

    return 0;
}

int xl9555_get_level(uint32_t pin_num_mask, uint32_t *level)
{
    esp_err_t esp_rt = ESP_OK;

    if (NULL == sg_xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "XL9555 I2C expander not initialized");
        return -1;
    }

    if (level == NULL) {
        ESP_LOGE(TAG, "Level pointer is NULL");
        return -1;
    }

    *level = 0;

    if (pin_num_mask & 0x000000FF) {
        uint8_t reg   = XL9555_INPUT_PORT_0_REG_ADDR;
        uint8_t value = 0;

        esp_rt = i2c_master_transmit_receive(sg_xl9555_config.xl9555_handle, &reg, 1, &value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read input port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }

        *level = (value & (pin_num_mask & 0x000000FF));
    }

    if (pin_num_mask & 0x0000FF00) {
        uint8_t reg   = XL9555_INPUT_PORT_1_REG_ADDR;
        uint8_t value = 0;

        esp_rt = i2c_master_transmit_receive(sg_xl9555_config.xl9555_handle, &reg, 1, &value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read input port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }
        *level |= (value & ((pin_num_mask >> 8) & 0xFF)) << 8;
    }

    return 0;
}

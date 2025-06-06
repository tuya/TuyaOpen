/**
 * @file xl9555.c
 * @brief xl9555 module is used to
 * @version 0.1
 * @date 2025-06-05
 */

#include "xl9555.h"

#include "board_config.h"

#if defined(BOARD_IO_EXPANDER_TYPE) && (BOARD_IO_EXPANDER_TYPE == IO_EXPANDER_TYPE_XL9555)

#include "esp_err.h"
#include "esp_log.h"

#include "driver/i2c_master.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "XL9555"

#ifndef IO_EXPANDER_XL9555_ADDR
// Default I2C address for XL9555
#define IO_EXPANDER_XL9555_ADDR (0x20)
#endif // IO_EXPANDER_XL9555_ADDR

// xl9555 input register address
#define XL9555_INPUT_PORT_0_REG_ADDR (0x00)
#define XL9555_INPUT_PORT_1_REG_ADDR (0x01)
// xl9555 output register address
#define XL9555_OUTPUT_PORT_0_REG_ADDR (0x02)
#define XL9555_OUTPUT_PORT_1_REG_ADDR (0x03)
// xl9555 polarity inversion register address
#define XL9555_POLARITY_INVERSION_PORT_0_REG_ADDR (0x04)
#define XL9555_POLARITY_INVERSION_PORT_1_REG_ADDR (0x05)
// xl9555 configuration register address
#define XL9555_CONFIGURATION_PORT_0_REG_ADDR (0x06)
#define XL9555_CONFIGURATION_PORT_1_REG_ADDR (0x07)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t xl9555_handle;
} XL9555_CONFIG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static XL9555_CONFIG_T xl9555_config = {0};

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
        // ESP_LOGI(TAG, "I2C bus handle retrieved successfully");
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

    return i2c_bus;
}

int xl9555_init(void)
{
    if (xl9555_config.xl9555_handle) {
        ESP_LOGI(TAG, "XL9555 I2C expander already initialized");
        return 0;
    }

    xl9555_config.i2c_bus = __i2c_init(I2C_NUM, I2C_SCL_IO, I2C_SDA_IO);
    if (!xl9555_config.i2c_bus) {
        ESP_LOGE(TAG, "Failed to initialize I2C bus");
        return -1;
    }

    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IO_EXPANDER_XL9555_ADDR,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(xl9555_config.i2c_bus, &i2c_device_cfg, &xl9555_config.xl9555_handle));
    if (NULL == xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "Failed to create XL9555 I2C expander");
        return -1;
    }
    ESP_LOGI(TAG, "XL9555 I2C expander initialized successfully");

    return 0;
}

int xl9555_set_dir(uint32_t pin_num_mask, int is_input)
{
    esp_err_t esp_rt = ESP_OK;
    uint8_t reg, value;

    if (NULL == xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "XL9555 I2C expander not initialized");
        return -1;
    }

    // ESP_LOGI(TAG, "Setting pin direction: pin_num_mask=0x%08X, is_input=%d", pin_num_mask, is_input);

    if (pin_num_mask & 0x000000FF) {
        reg = XL9555_CONFIGURATION_PORT_0_REG_ADDR;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read configuration port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }
        // ESP_LOGI(TAG, "Read configuration port 0: 0x%02X", read_value);

        if (is_input) {
            value = read_value | (pin_num_mask & 0x000000FF);
        } else {
            value = read_value & ~(pin_num_mask & 0x000000FF);
        }
        // ESP_LOGI(TAG, "Setting configuration port 0 to: 0x%02X", value);

        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(xl9555_config.xl9555_handle, buffer, 2, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set configuration port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }
    }

    if (pin_num_mask & 0x0000FF00) {
        reg = XL9555_CONFIGURATION_PORT_1_REG_ADDR;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read configuration port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }
        // ESP_LOGI(TAG, "Read configuration port 1: 0x%02X", read_value);

        if (is_input) {
            value = read_value | ((pin_num_mask >> 8) & 0xFF);
        } else {
            value = read_value & ~((pin_num_mask >> 8) & 0xFF);
        }
        // ESP_LOGI(TAG, "Setting configuration port 1 to: 0x%02X", value);

        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(xl9555_config.xl9555_handle, buffer, 2, 100);
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

    if (NULL == xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "XL9555 I2C expander not initialized");
        return -1;
    }

    if (pin_num_mask & 0x000000FF) {
        uint8_t reg = XL9555_OUTPUT_PORT_0_REG_ADDR;
        uint8_t value = 0;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read output port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }
        // ESP_LOGI(TAG, "Read output port 0: 0x%02X", read_value);

        if (level) {
            value = read_value | (pin_num_mask & 0x000000FF);
        } else {
            value = read_value & (~(pin_num_mask & 0x000000FF));
        }
        // ESP_LOGI(TAG, "Setting output port 0 to: 0x%02X", value);
        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(xl9555_config.xl9555_handle, buffer, 2, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set output port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }
    }

    if (pin_num_mask & 0x0000FF00) {
        uint8_t reg = XL9555_OUTPUT_PORT_1_REG_ADDR;
        uint8_t value = 0;

        uint8_t read_value = 0;
        esp_rt = i2c_master_transmit_receive(xl9555_config.xl9555_handle, &reg, 1, &read_value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read output port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }
        // ESP_LOGI(TAG, "Read output port 1: 0x%02X", read_value);

        if (level) {
            value = read_value | ((pin_num_mask >> 8) & 0xFF);
        } else {
            value = read_value & (~((pin_num_mask >> 8) & 0xFF));
        }
        // ESP_LOGI(TAG, "Setting output port 1 to: 0x%02X", value);
        uint8_t buffer[2] = {reg, value};
        esp_rt = i2c_master_transmit(xl9555_config.xl9555_handle, buffer, 2, 100);
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

    if (NULL == xl9555_config.xl9555_handle) {
        ESP_LOGE(TAG, "XL9555 I2C expander not initialized");
        return -1;
    }

    if (level == NULL) {
        ESP_LOGE(TAG, "Level pointer is NULL");
        return -1;
    }

    *level = 0;

    if (pin_num_mask & 0x000000FF) {
        uint8_t reg = XL9555_INPUT_PORT_0_REG_ADDR;
        uint8_t value = 0;

        esp_rt = i2c_master_transmit_receive(xl9555_config.xl9555_handle, &reg, 1, &value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read input port 0: %s", esp_err_to_name(esp_rt));
            return -1;
        }

        *level = (value & (pin_num_mask & 0x000000FF));
    }

    if (pin_num_mask & 0x0000FF00) {
        uint8_t reg = XL9555_INPUT_PORT_1_REG_ADDR;
        uint8_t value = 0;

        esp_rt = i2c_master_transmit_receive(xl9555_config.xl9555_handle, &reg, 1, &value, 1, 100);
        if (esp_rt != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read input port 1: %s", esp_err_to_name(esp_rt));
            return -1;
        }
        *level |= (value & ((pin_num_mask >> 8) & 0xFF)) << 8;
    }

    return 0;
}

#endif // IO_EXPANDER_TYPE_XL9555
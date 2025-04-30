#include "esp_check.h"
#include "driver/i2c_master.h"

#include "tal_log.h"
#include "tal_memory.h"

#include "tdd_xl9555_io.h"

#define IIC_SPEED_CLK 100000 /* Speed 100K */

static i2c_master_dev_handle_t xl9555_handle = NULL;

/**
 * @brief Read data from XL9555 registers
 * 
 * @param data buffer pointer
 * @param len buffer length
 */
static void xl9555_read_byte(uint8_t *data, size_t len)
{
    esp_err_t err = ESP_OK;
    uint8_t reg_addr = XL9555_INPUT_PORT0_REG;

    err = i2c_master_transmit_receive(xl9555_handle, &reg_addr, 1, data, len, -1);
    if (err != ESP_OK) {
        PR_ERR("i2c_master_transmit_receive error: %d", err);
    }
}

/**
 * @brief Write data to XL9555 registers
 * 
 * @param reg register address
 * @param data buffer pointer
 * @param len buffer length
 */
static void xl9555_write_byte(uint8_t reg, uint8_t *data, size_t len)
{
    esp_err_t err = ESP_OK;
    size_t size = len + 1;
    uint8_t *buf = NULL;
    
    buf = tal_malloc(size);
    if (buf == NULL) {
        PR_ERR("tal_malloc error");
        return;
    }
    memset(buf, 0, size);

    buf[0] = reg;                   /* Register address */
    memcpy(buf + 1, data, len);     /* Register data */
    
    err = i2c_master_transmit(xl9555_handle, buf, size, -1);
    if (err != ESP_OK) {
        PR_ERR("i2c_master_transmit error: %d, reg: %02x", err, reg);
    }

    free(buf);

    return;
}

static void xl9555_ioconfig(uint16_t value)
{
    uint8_t data[2] = {0};

    data[0] = (uint8_t)(0xFF & value);
    data[1] = (uint8_t)(0xFF & (value >> 8));

    xl9555_write_byte(XL9555_CONFIG_PORT0_REG, data, 2);
    
    return;
}

OPERATE_RET tdd_xl9555_io_init(void *handle, uint16_t config)
{
    i2c_master_bus_handle_t i2c_bus_handle = (i2c_master_bus_handle_t)handle;

    uint8_t r_data[2] = {0};

    if (i2c_bus_handle == NULL) {
        PR_ERR("I2C bus handle is NULL");
        return OPRT_COM_ERROR;
    }
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,  /* Select Slave address length 7 */
        .scl_speed_hz    = IIC_SPEED_CLK,       /* Transfer rate */
        .device_address  = XL9555_ADDR,         /* 7-bit slave address */
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &xl9555_handle));
    
    /* Read once after power-up to clear interrupt flag */
    xl9555_read_byte(r_data, 2);
    /* Configure which expansion pins are input/output mode */
    xl9555_ioconfig(config);
    
    return OPRT_OK;
}

OPERATE_RET tdd_xl9555_io_set(uint16_t pin, int val)
{
    uint8_t w_data[2];
    uint16_t mask = 0x0000;

    PR_DEBUG("xl9555 IO pin: %04x, val: %d", pin, val);

    xl9555_read_byte(w_data, 2);

    if (pin <= 0x0080) {
        if (val) {
            w_data[0] |= (uint8_t)(0xFF & pin);
        } else {
            w_data[0] &= ~(uint8_t)(0xFF & pin);
        }
    } else {
        if (val) {
            w_data[1] |= (uint8_t)(0xFF & (pin >> 8));
        } else {
            w_data[1] &= ~(uint8_t)(0xFF & (pin >> 8));
        }
    }

    mask = ((uint16_t)w_data[1] << 8) | w_data[0]; 
    PR_INFO("xl9555 IO mask: %04x", mask);

    xl9555_write_byte(XL9555_OUTPUT_PORT0_REG, w_data, 2);

    return OPRT_OK;
}

OPERATE_RET tdd_xl9555_io_get(uint16_t pin, int *val)
{
    uint16_t mask = 0;
    uint8_t r_data[2] = {0};

    if (val == NULL) {
        PR_ERR("val is NULL");
        return OPRT_INVALID_PARM;
    }

    xl9555_read_byte(r_data, 2);

    mask = r_data[1] << 8 | r_data[0];

    *val = (mask & pin) ? 1 : 0;

    return OPRT_OK;
}
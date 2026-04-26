#include "BH1750.h"
#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_system.h"

#define BH1750_CMD_POWER_ON       0x01
#define BH1750_CMD_RESET          0x07
#define BH1750_CMD_ONE_H_RES_MODE 0x20

OPERATE_RET BH1750_Init(void)
{
    tkl_io_pinmux_config(BH1750_SCL_PIN, TUYA_IIC2_SCL);
    tkl_io_pinmux_config(BH1750_SDA_PIN, TUYA_IIC2_SDA);

    TUYA_IIC_BASE_CFG_T cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT,
    };
    tkl_i2c_init(BH1750_I2C_PORT, &cfg);

    uint8_t cmd = BH1750_CMD_POWER_ON;
    OPERATE_RET ret = tkl_i2c_master_send(BH1750_I2C_PORT, BH1750_I2C_ADDR, &cmd, 1, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("BH1750 power on failed: %d", ret);
        return ret;
    }

    tkl_system_sleep(10);
    cmd = BH1750_CMD_RESET;
    ret = tkl_i2c_master_send(BH1750_I2C_PORT, BH1750_I2C_ADDR, &cmd, 1, FALSE);
    if (ret != OPRT_OK) {
        PR_WARN("BH1750 reset failed: %d", ret);
    }

    return OPRT_OK;
}

OPERATE_RET BH1750_Read_Light(uint16_t *light)
{
    uint8_t cmd = BH1750_CMD_ONE_H_RES_MODE;
    uint8_t data[2] = {0};

    if (light == NULL) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = tkl_i2c_master_send(BH1750_I2C_PORT, BH1750_I2C_ADDR, &cmd, 1, FALSE);
    if (ret != OPRT_OK) {
        return ret;
    }

    tkl_system_sleep(180);

    ret = tkl_i2c_master_receive(BH1750_I2C_PORT, BH1750_I2C_ADDR, data, sizeof(data), FALSE);
    if (ret == OPRT_OK) {
        uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
        *light = (uint16_t)((raw * 10U + 6U) / 12U);
        PR_DEBUG("BH1750 read ok: raw=%u light=%u", raw, *light);
    }
    return ret;
}

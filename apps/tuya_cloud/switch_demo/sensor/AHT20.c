/****************************************************************************
* | File      	:   AHT20.c
* | Function    :   AHT20温湿度传感器驱动实现（最小化版本）
******************************************************************************/
#include "AHT20.h"
#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_system.h"
#include <stdio.h>
#include <string.h>

// CRC8计算函数（基于MAXIM多项式）
static uint8_t AHT20_Calc_CRC8(uint8_t *message, uint8_t num) {
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;
    
    for (byte = 0; byte < num; byte++) {
        crc ^= (message[byte]);
        for (i = 8; i > 0; --i) {
            if (crc & 0x80) 
                crc = (crc << 1) ^ 0x31;
            else 
                crc = (crc << 1);
        }
    }
    return crc;
}

static OPERATE_RET AHT20_Parse_Data(uint8_t *data, float *temperature, float *humidity)
{
    uint32_t hum_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
    uint32_t temp_raw = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    *humidity = (float)hum_raw * 100.0f / 1048576.0f;
    *temperature = (float)temp_raw * 200.0f / 1048576.0f - 50.0f;

    if (*humidity < 0.0f || *humidity > 100.0f || *temperature < -40.0f || *temperature > 85.0f) {
        PR_WARN("AHT20 value out of range: temp=%.1f hum=%.1f", *temperature, *humidity);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

// 读取AHT20状态寄存器
OPERATE_RET AHT20_Read_Status(uint8_t *status) {
    OPERATE_RET op_ret = OPRT_OK;
    
    op_ret = tkl_i2c_master_receive(AHT20_I2C_PORT, AHT20_I2C_ADDR, status, 1, FALSE);
    if (OPRT_OK != op_ret) {
        PR_ERR("读取AHT20状态失败, err<%d>!", op_ret);
        return op_ret;
    }
    
    return OPRT_OK;
}

// 检查校准状态
OPERATE_RET AHT20_Check_Calibration(void) {
    OPERATE_RET op_ret = OPRT_OK;
    uint8_t status = 0;
    
    op_ret = AHT20_Read_Status(&status);
    if (OPRT_OK != op_ret) {
        return op_ret;
    }
    
    // 检查校准使能位
    if ((status & AHT20_STATUS_CAL) == AHT20_STATUS_CAL) {
        return OPRT_OK;  // 校准已使能
    } else {
        PR_ERR("AHT20校准未使能, 状态: 0x%02X", status);
        return OPRT_COM_ERROR;  // 确保函数有返回值
    }
}

// AHT20初始化（简化版）
OPERATE_RET AHT20_Init(void) {
    OPERATE_RET op_ret = OPRT_OK;
    uint8_t reset_cmd = AHT20_CMD_SOFTRESET;
    uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
    uint8_t status = 0;
    
    tkl_io_pinmux_config(AHT20_SCL_PIN, TUYA_IIC2_SCL);
    tkl_io_pinmux_config(AHT20_SDA_PIN, TUYA_IIC2_SDA);
    
    TUYA_IIC_BASE_CFG_T cfg = {
        .role = TUYA_IIC_MODE_MASTER,
        .speed = TUYA_IIC_BUS_SPEED_100K,
        .addr_width = TUYA_IIC_ADDRESS_7BIT
    };
    
    op_ret = tkl_i2c_init(AHT20_I2C_PORT, &cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("AHT20 I2C init failed: %d", op_ret);
        return op_ret;
    }

    tkl_i2c_master_send(AHT20_I2C_PORT, AHT20_I2C_ADDR, &reset_cmd, 1, FALSE);
    tkl_system_sleep(40);
    
    op_ret = tkl_i2c_master_send(AHT20_I2C_PORT, AHT20_I2C_ADDR, init_cmd, 3, FALSE);
    if (OPRT_OK != op_ret) {
        PR_ERR("AHT20 init cmd failed: %d", op_ret);
        return op_ret;
    }
    
    tkl_system_sleep(10);
    op_ret = AHT20_Read_Status(&status);
    if (OPRT_OK == op_ret) {
        PR_NOTICE("AHT20 status: 0x%02X", status);
    }

    return op_ret;
}

// 读取温湿度数据（最小化版本）
OPERATE_RET AHT20_Read_Data(float *temperature, float *humidity) {
    return AHT20_Read_Data_CRC(temperature, humidity);
}

// 软复位（简化版）
OPERATE_RET AHT20_SoftReset(void) {
    uint8_t reset_cmd = AHT20_CMD_SOFTRESET;
    OPERATE_RET op_ret = tkl_i2c_master_send(AHT20_I2C_PORT, AHT20_I2C_ADDR, &reset_cmd, 1, FALSE);
    if (OPRT_OK != op_ret) return op_ret;
    tkl_system_sleep(20);
    return OPRT_OK;
}

// 读取温湿度数据（带CRC校验版本）
OPERATE_RET AHT20_Read_Data_CRC(float *temperature, float *humidity) {
    OPERATE_RET op_ret = OPRT_OK;
    uint8_t data[7] = {0};
    uint8_t status = 0;
    uint8_t measure_cmd[3] = {AHT20_CMD_MEASURE, 0x33, 0x00};

    if (temperature == NULL || humidity == NULL) {
        return OPRT_INVALID_PARM;
    }
    
    op_ret = AHT20_Read_Status(&status);
    if (OPRT_OK != op_ret) {
        return op_ret;
    }

    if ((status & AHT20_STATUS_CAL) == 0) {
        uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
        op_ret = tkl_i2c_master_send(AHT20_I2C_PORT, AHT20_I2C_ADDR, init_cmd, 3, FALSE);
        if (OPRT_OK != op_ret) {
            return op_ret;
        }
        tkl_system_sleep(10);
    }
    
    op_ret = tkl_i2c_master_send(AHT20_I2C_PORT, AHT20_I2C_ADDR, measure_cmd, 3, FALSE);
    if (OPRT_OK != op_ret) {
        return op_ret;
    }
    
    tkl_system_sleep(80);
    for (int timeout = 20; timeout > 0; timeout--) {
        op_ret = AHT20_Read_Status(&status);
        if (OPRT_OK != op_ret) {
            return op_ret;
        }
        if ((status & AHT20_STATUS_BUSY) == 0) {
            break;
        }
        tkl_system_sleep(5);
    }

    if (status & AHT20_STATUS_BUSY) {
        PR_WARN("AHT20 busy timeout, status: 0x%02X", status);
        return OPRT_TIMEOUT;
    }
    
    op_ret = tkl_i2c_master_receive(AHT20_I2C_PORT, AHT20_I2C_ADDR, data, sizeof(data), FALSE);
    if (OPRT_OK != op_ret) {
        return op_ret;
    }
    
    uint8_t crc_calculated = AHT20_Calc_CRC8(data, 6);
    if (crc_calculated != data[6]) {
        PR_WARN("AHT20 CRC failed, fallback to raw parse: calc=0x%02X read=0x%02X raw=%02X %02X %02X %02X %02X %02X %02X",
                crc_calculated, data[6], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    }

    op_ret = AHT20_Parse_Data(data, temperature, humidity);
    if (op_ret != OPRT_OK) {
        return op_ret;
    }
    
    PR_DEBUG("AHT20 read ok: temp=%.1f hum=%.1f", *temperature, *humidity);
    return OPRT_OK;
}
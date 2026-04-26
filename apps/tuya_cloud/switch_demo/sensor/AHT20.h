#ifndef AHT20_H
#define AHT20_H

#include "tuya_cloud_types.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#define AHT20_I2C_PORT TUYA_I2C_NUM_2
#define AHT20_I2C_ADDR 0x38
#define AHT20_SCL_PIN  TUYA_GPIO_NUM_14
#define AHT20_SDA_PIN  TUYA_GPIO_NUM_15

#define AHT20_CMD_INIT      0xBE
#define AHT20_CMD_MEASURE   0xAC
#define AHT20_CMD_SOFTRESET 0xBA
#define AHT20_STATUS_BUSY   0x80
#define AHT20_STATUS_CAL    0x08

OPERATE_RET AHT20_Init(void);
OPERATE_RET AHT20_Read_Data(float *temperature, float *humidity);
OPERATE_RET AHT20_Read_Data_CRC(float *temperature, float *humidity);
OPERATE_RET AHT20_SoftReset(void);

#endif
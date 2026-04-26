#ifndef BH1750_H
#define BH1750_H

#include "tuya_cloud_types.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"

#define BH1750_I2C_PORT TUYA_I2C_NUM_2
#define BH1750_I2C_ADDR 0x23
#define BH1750_SCL_PIN  TUYA_GPIO_NUM_14
#define BH1750_SDA_PIN  TUYA_GPIO_NUM_15

OPERATE_RET BH1750_Init(void);
OPERATE_RET BH1750_Read_Light(uint16_t *light);

#endif

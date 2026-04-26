#ifndef OLED_H
#define OLED_H

#include "tuya_cloud_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_I2C_PORT TUYA_I2C_NUM_0
#define OLED_I2C_ADDR 0x3C
#define OLED_SCL_PIN  20
#define OLED_SDA_PIN  21
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_PAGES    8

OPERATE_RET OLED_Init(void);
OPERATE_RET OLED_Clear(void);
OPERATE_RET OLED_ShowString(uint8_t page, uint8_t column, const char *str);

#ifdef __cplusplus
}
#endif

#endif

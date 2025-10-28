/*
 * tuya_ai_battery.h
 * Copyright (C) 2025 cc <cc@tuya>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef __TUYA_AI_BATTERY_H__
#define __TUYA_AI_BATTERY_H__

#include "stdint.h"
#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "tuya_app_config.h"
#include "tuya_device_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

// 电池电量获取方式，ADC 或者 IIC
typedef enum {
    AI_BATTERY_MODE_ADC = 0,
    AI_BATTERY_MODE_IIC,
    AI_BATTERY_MODE_MAX,
} ai_battery_mode_t;

// 外部电池电量回调处理
typedef int (*ai_battery_callback)(uint8_t current_capacity);

// i2c读取电池电量
typedef int (*ai_battery_i2c_init)(void);
typedef int (*ai_battery_i2c_deinit)(void);
typedef int (*ai_battery_get_capacity_by_i2c)(uint8_t battery_current_capacity);

// ADC 方式获取电池电量，电压/容量对照表<电池放电曲线>
typedef struct {
    uint32_t v;
    uint32_t c;
} voltage_cap_map;

typedef struct {
    uint8_t adc_gpio;
    uint8_t map_size;
    voltage_cap_map *map;
} ai_battery_get_capacity_by_adc;

typedef struct {
    ai_battery_i2c_init init;
    ai_battery_i2c_deinit deinit;
    ai_battery_get_capacity_by_i2c get_capacity;
} ai_battery_get_capacity_by_iic;

typedef struct {
    ai_battery_mode_t mode;
    union {
        ai_battery_get_capacity_by_adc adc;
        ai_battery_get_capacity_by_iic i2c;
    };
    ai_battery_callback cb;
    uint8_t (*is_charging_cb)(void);
} ai_battery_conf_t;

typedef VOID (*TY_BATTERY_CB)(BOOL_T is_low, BOOL_T is_charging);
/***********************************************************
********************function declaration********************
***********************************************************/


/**
 * @brief 
 * 
 */
VOID tuya_ai_battery_force_upload(VOID);

/**
 * @brief 
 * 
 */
UINT8_T tuya_ai_battery_get_capacity(VOID);

/**
 * @brief 
 * 
 */
OPERATE_RET tuya_ai_toy_battery_init(VOID);

/**
 * @brief 
 * 
 */
OPERATE_RET tuya_ai_toy_battery_uninit(void);


#ifdef __cplusplus
}
#endif

#endif /* !__TUYA_AI_BATTERY_H__ */


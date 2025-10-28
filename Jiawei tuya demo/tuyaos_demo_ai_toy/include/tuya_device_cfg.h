/**
 * @file tuya_device_cfg.h
 * @brief
 * @version 0.1
 * @date 2023-06-26
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */

#ifndef __TUYA_DEVICE_CFG_H__
#define __TUYA_DEVICE_CFG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_app_config.h"
#include "tuya_ai_toy.h"

#ifndef TY_SPK_DEFAULT_VOL
#define TY_SPK_DEFAULT_VOL 70
#endif

// default language type: 0: chinese, 1: english
#define TY_AI_DEFAULT_LANG 1

// default lowpower mode, default is deepsleep, use audio trigger pin as wakeup pin 
// if dont need spport lowpower mode, remove this macro.
// #define TY_AI_DEFAULT_LOWP_MODE TUYA_CPU_SLEEP

// enable ai monitor, default not support, the tool not ready now
#define ENABLE_APP_AI_MONITOR 0

// enable ai audio analysis
#define ENABLE_AUDIO_ANALYSIS 0

#define ENABLE_CLOUD_ALERT 0

#if ENABLE_CLOUD_ALERT
#define TY_AI_ALERT_DELAY_MS 0
#else
#define TY_AI_ALERT_DELAY_MS 500
#endif

/**
 * @brief Firmware info
 *
 */
// #define GFW_FIRMWARE_KEY "keyemx4vfwsx58ud"
#define GFW_FIRMWARE_KEY "keyfqaeurftaekfg"
//T5AIborad，要调整这里
#if defined(T5AI_BOARD) && T5AI_BOARD == 1
// default pins
#define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_12//改为无用的GPIO口，原来是TUYA_GPIO_NUM_12
#define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_28
#define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_1

// default mode
#define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_FREE

// default display
#define LCD_DEV_NAME     "ili9488"
#define LCD_WIDTH        320
#define LCD_HEIGHT       480
#define LCD_ROTATION     TKL_DISP_ROTATION_0
#define LCD_FPS          10

// enable ai opus encode
#define ENABLE_APP_OPUS_ENCODER 0
#elif  defined(T5AI_BOARD_EYES) && T5AI_BOARD_EYES == 1
// default pins
#define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_12
#define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_28
#define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_1

// default mode
#define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_HOLD

// default display
#define LCD_DEV_NAME     "st7735s"
#define LCD_WIDTH        128
#define LCD_HEIGHT       128
#define LCD_ROTATION     TKL_DISP_ROTATION_180
#define LCD_FPS          10

// enable ai opus encode
#define ENABLE_APP_OPUS_ENCODER 0
#elif defined(T5AI_BOARD_EVB) && T5AI_BOARD_EVB == 1
// default battery
#define TUYA_AI_TOY_BATTERY_ENABLE 1
#define TUYA_AI_TOY_CHARGE_PIN          TUYA_GPIO_NUM_21
#define TUYA_AI_TOY_BATTERY_CAP_PIN     TUYA_GPIO_NUM_28

// default pins
#define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_4
#define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_19
#define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_8

// default mode
#define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_HOLD

// default display
#define LCD_DEV_NAME     "st7789"
#define LCD_WIDTH        240
#define LCD_HEIGHT       240
#define LCD_ROTATION     TKL_DISP_ROTATION_0
#define LCD_FPS          15

// enable ai opus encode
#define ENABLE_APP_OPUS_ENCODER 1
#elif defined(T5AI_BOARD_EVB_PRO) && T5AI_BOARD_EVB_PRO == 1
// default battery
#define TUYA_AI_TOY_BATTERY_ENABLE 1
#define TUYA_AI_TOY_CHARGE_PIN  TUYA_GPIO_NUM_21
#define TUYA_AI_TOY_BATTERY_CAP_PIN TUYA_GPIO_NUM_28

// default pins
#define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_6
#define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_26
#define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_25

// default mode
#define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_HOLD

// default display
#define LCD_DEV_NAME     "st7789"
#define LCD_WIDTH        240
#define LCD_HEIGHT       240
#define LCD_ROTATION     TKL_DISP_ROTATION_0
#define LCD_FPS          15

// enable ai opus encode
#define ENABLE_APP_OPUS_ENCODER 1
#elif defined(T5AI_BOARD_ROBOT) && T5AI_BOARD_ROBOT == 1
// default battery
#define TUYA_AI_TOY_BATTERY_ENABLE 1
#define TUYA_AI_TOY_CHARGE_PIN  TUYA_GPIO_NUM_21
#define TUYA_AI_TOY_BATTERY_CAP_PIN TUYA_GPIO_NUM_28

// default pins
#define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_5
#define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_26
#define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_MAX

// default mode
#define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_ONE_SHOT

// default display
#define LCD_DEV_NAME     "st7789p3"
#define LCD_WIDTH        320
#define LCD_HEIGHT       172
#define LCD_ROTATION     TKL_DISP_ROTATION_0
#define LCD_FPS          10

// enable ai opus encode
#define ENABLE_APP_OPUS_ENCODER 0
#undef TY_AI_DEFAULT_LOWP_MODE
#elif defined(T5AI_BOARD_CELLULAR) && T5AI_BOARD_CELLULAR == 1
// default battery
#define TUYA_AI_TOY_BATTERY_ENABLE 1
#define TUYA_AI_TOY_CHARGE_PIN  TUYA_GPIO_NUM_20
#define TUYA_AI_TOY_BATTERY_CAP_PIN TUYA_GPIO_NUM_23

// default pins
#define TUYA_AI_TOY_AUDIO_TRIGGER_PIN   TUYA_GPIO_NUM_47
#define TUYA_AI_TOY_SPK_EN_PIN          TUYA_GPIO_NUM_45
#define TUYA_AI_TOY_LED_PIN             TUYA_GPIO_NUM_7

// default mode
#define TUYA_AI_TOY_DEFAULT_CHAT_MODE   TY_AI_TRIGGER_MODE_HOLD

// enable ai opus encode
#define ENABLE_APP_OPUS_ENCODER 1
#endif

#define TY_AI_TOY_CFG_DEFAULT { \
    .audio_trigger_pin = TUYA_AI_TOY_AUDIO_TRIGGER_PIN, \
    .spk_en_pin = TUYA_AI_TOY_SPK_EN_PIN, \
    .led_pin = TUYA_AI_TOY_LED_PIN, \
    .trigger_mode = TUYA_AI_TOY_DEFAULT_CHAT_MODE, \
}

#ifdef __cplusplus
}
#endif

#endif // __TUYA_DEVICE_CFG_H__

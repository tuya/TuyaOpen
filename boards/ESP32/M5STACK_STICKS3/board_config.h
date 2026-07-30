/**
 * @file board_config.h
 * @brief Board configuration for M5Stack StickS3.
 * @version 0.1
 * @date 2026-04-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "sdkconfig.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* Shared internal I2C bus: ES8311, BMI270, M5PM1. */
#define I2C_NUM    TUYA_I2C_NUM_1
#define I2C_SCL_IO TUYA_GPIO_NUM_48
#define I2C_SDA_IO TUYA_GPIO_NUM_47

/* ES8311 audio codec. Speaker amp SHDN is controlled by M5PM1 PYG3, not ESP32 GPIO. */
#define I2S_NUM    TUYA_I2S_NUM_1
#define I2S_MCK_IO TUYA_GPIO_NUM_18
#define I2S_BCK_IO TUYA_GPIO_NUM_17
#define I2S_WS_IO  TUYA_GPIO_NUM_15
#define I2S_DO_IO  TUYA_GPIO_NUM_14
#define I2S_DI_IO  TUYA_GPIO_NUM_16

#define GPIO_OUTPUT_PA (-1)

#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR_7BIT (0x18)
#define AUDIO_CODEC_ES8311_ADDR      (AUDIO_CODEC_ES8311_ADDR_7BIT << 1)

#ifndef DISPLAY_NAME
#define DISPLAY_NAME "st7789_spi"
#endif

/* ST7789P3 LCD. */
#define LCD_MOSI_PIN TUYA_GPIO_NUM_39
#define LCD_SCLK_PIN TUYA_GPIO_NUM_40
#define LCD_DC_PIN   TUYA_GPIO_NUM_45
#define LCD_CS_PIN   TUYA_GPIO_NUM_41
#define LCD_RST_PIN  TUYA_GPIO_NUM_21

#define DISPLAY_BACKLIGHT_PIN           TUYA_GPIO_NUM_38

#define DISPLAY_WIDTH  (135)
#define DISPLAY_HEIGHT (240)
#define DISPLAY_OFFSET_X (52)
#define DISPLAY_OFFSET_Y (40)

/* Rotation. Panel orientation is set for the LVGL render path (the one real apps use):
 * no mirroring. The bring-up example uses direct framebuffer draws and flips Y itself. */
#define DISPLAY_SWAP_XY  false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

#define DISPLAY_COLOR_INVERT true

#define DISPLAY_SWAP_BYTES 1

/* M5PM1 power controller.
 * StickS3 power levels are independent switches sourced from L0:
 * L1 uses LDO3V3_EN_PP for the IMU, L2/L3A uses DCDC3V3_EN_PP for
 * ESP32-S3-side power, EXT_5V uses BOOST5V_EN_PP, and L3B uses PYG2 for
 * LCD backlight, microphone, and speaker peripheral power.
 */
#define M5PM1_I2C_PORT I2C_NUM
#define M5PM1_I2C_ADDR (0x6E)

#define M5PM1_GPIO_CHARGE_STATUS (0)
#define M5PM1_GPIO_L3B_POWER     (2)
#define M5PM1_GPIO_SPK_AMP_SHDN  (3)
#define M5PM1_GPIO_IMU_INT1      (4)

#define M5PM1_L3B_POWER_ENABLE_LEVEL true
#define M5PM1_L3B_POWER_DISABLE_LEVEL false
#define M5PM1_SPK_AMP_ENABLE_LEVEL   true
#define M5PM1_SPK_AMP_DISABLE_LEVEL  false

/* BMI270 IMU. Powered by 3V3_L1 (LDO). Shares the internal I2C bus. */
#define BMI270_I2C_PORT    I2C_NUM
#define BMI270_I2C_ADDR    BMI2_I2C_PRIM_ADDR  /* 0x68, ADDR pin = low */
#define BMI270_I2C_ADDR_ALT BMI2_I2C_SEC_ADDR  /* 0x69, ADDR pin = high */

/* IR transmitter / receiver. */
#define IR_TX_IO    TUYA_GPIO_NUM_46
#define IR_RX_IO    TUYA_GPIO_NUM_42

/* Buttons. */
#define BOARD_BUTTON_PIN         TUYA_GPIO_NUM_11
#define BOARD_BUTTON_2_PIN       TUYA_GPIO_NUM_12
#define BOARD_BUTTON_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_2_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#ifndef BUTTON_NAME_2
#define BUTTON_NAME_2 "ai_chat_button_2"
#endif

#ifdef __cplusplus
}
#endif
#endif /* __BOARD_CONFIG_H__ */

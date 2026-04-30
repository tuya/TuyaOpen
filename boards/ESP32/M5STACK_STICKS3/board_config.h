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

/* Display type. */
#define DISPLAY_TYPE_UNKNOWN        0
#define DISPLAY_TYPE_OLED_SSD1306   1
#define DISPLAY_TYPE_LCD_SH8601     2
#define DISPLAY_TYPE_LCD_ST7789_80  3
#define DISPLAY_TYPE_LCD_ST7789_SPI 4

#define BOARD_DISPLAY_TYPE DISPLAY_TYPE_LCD_ST7789_SPI

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
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define DISPLAY_WIDTH  (135)
#define DISPLAY_HEIGHT (240)
#define DISPLAY_OFFSET_X (52)
#define DISPLAY_OFFSET_Y (40)

/* LVGL config. */
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * 20)

#define DISPLAY_MONOCHROME false

/* Rotation. */
#define DISPLAY_SWAP_XY  false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

#define DISPLAY_COLOR_FORMAT LV_COLOR_FORMAT_RGB565
#define DISPLAY_COLOR_INVERT true

/* Only one of DISPLAY_BUFF_SPIRAM and DISPLAY_BUFF_DMA can be selected. */
#define DISPLAY_BUFF_SPIRAM 0
#define DISPLAY_BUFF_DMA    1

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
#define M5PM1_GPIO_IRQ           (1)
#define M5PM1_GPIO_L3B_POWER     (2)
#define M5PM1_GPIO_SPK_AMP_SHDN  (3)
#define M5PM1_GPIO_IMU_INT1      (4)

#define M5PM1_L3B_POWER_ENABLE_LEVEL true
#define M5PM1_L3B_POWER_DISABLE_LEVEL false
#define M5PM1_SPK_AMP_ENABLE_LEVEL   true
#define M5PM1_SPK_AMP_DISABLE_LEVEL  false

/* Buttons. */
#define BOARD_BUTTON_PIN         TUYA_GPIO_NUM_11
#define BOARD_BUTTON_2_PIN       TUYA_GPIO_NUM_12
#define BOARD_BUTTON_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW
#define BOARD_BUTTON_2_ACTIVE_LV TUYA_GPIO_LEVEL_LOW

#ifndef BUTTON_NAME_2
#define BUTTON_NAME_2 "ai_chat_button_2"
#endif

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Show a simple centered red box on the board display.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_display_show_red_box(void);

/**
 * @brief Enable or disable StickS3 L1 IMU power.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l1(bool enable);

/**
 * @brief Enable or disable StickS3 L2/L3A ESP32-S3-side power switch.
 * @param[in] enable true to enable, false to disable.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l2_l3a(bool enable);

/**
 * @brief Enable or disable StickS3 EXT_5V output mode.
 * @param[in] enable true for 5V output mode, false for 5V input mode.
 * @return OPRT_OK on success, error code on failure.
 * @attention Only enable output mode when external 5V is not supplied through Grove/Hat EXT_5V/5VIN.
 */
OPERATE_RET board_sticks3_power_set_ext_5v_output(bool enable);

/**
 * @brief Enable or disable StickS3 L3B peripheral power.
 * @param[in] enable true to power LCD backlight, microphone, and speaker peripherals.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l3b(bool enable);

/**
 * @brief Enable or disable StickS3 speaker amplifier.
 * @param[in] enable true to enable amplifier, false to disable amplifier.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_speaker_amp(bool enable);

/**
 * @brief Enable or disable StickS3 L1 IMU power hold while M5PM1 sleeps.
 * @param[in] enable true to keep L1 held during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l1_hold(bool enable);

/**
 * @brief Enable or disable StickS3 EXT_5V power hold while M5PM1 sleeps.
 * @param[in] enable true to keep EXT_5V held during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 * @attention Only hold EXT_5V output when external 5V is not supplied through Grove/Hat EXT_5V/5VIN.
 */
OPERATE_RET board_sticks3_power_set_ext_5v_hold(bool enable);

/**
 * @brief Enable or disable StickS3 L3B peripheral power hold while M5PM1 sleeps.
 * @param[in] enable true to hold L3B state during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_l3b_hold(bool enable);

/**
 * @brief Enable or disable StickS3 speaker amplifier hold while M5PM1 sleeps.
 * @param[in] enable true to hold speaker amplifier state during PMIC sleep.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_speaker_amp_hold(bool enable);

/**
 * @brief Read and optionally clear StickS3 M5PM1 wake source flags.
 * @param[out] wake_source wake source bitmask from M5PM1.
 * @param[in] clear_after_read true to clear the returned flags.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_get_wake_source(uint8_t *wake_source, bool clear_after_read);

/**
 * @brief Configure a timer wake before entering M5PM1 shutdown.
 * @param[in] seconds wake timer in seconds.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_set_timer_wake(uint32_t seconds);

/**
 * @brief Clear the M5PM1 timer wake configuration.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_clear_timer_wake(void);

/**
 * @brief Configure BMI270 INT1 via M5PM1 PYG4 as a PMIC wake source.
 * @param[in] enable true to enable IMU wake source, false to disable it.
 * @param[in] rising_edge true for rising-edge wake, false for falling-edge wake.
 * @return OPRT_OK on success, error code on failure.
 * @note The BMI270 itself must be configured separately before this wake source can fire.
 */
OPERATE_RET board_sticks3_power_set_imu_wake(bool enable, bool rising_edge);

/**
 * @brief Request StickS3 PMIC shutdown.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_shutdown(void);

/**
 * @brief Request StickS3 PMIC reboot.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_sticks3_power_reboot(void);

/**
 * @brief Enable all currently supported StickS3 board power rails.
 * @return OPRT_OK on success, error code on failure.
 * @attention This enables EXT_5V output mode for bring-up. Do not feed external 5V into output interfaces.
 */
OPERATE_RET board_sticks3_power_enable_all(void);

#ifdef __cplusplus
}
#endif
#endif /* __BOARD_CONFIG_H__ */

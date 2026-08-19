/**
 * @file board_com_api.h
 * @brief Common board-level hardware registration APIs for NiceMCU-T5-0.96ISP
 * @version 0.2
 * @date 2026-08-10
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_COM_API_H__
#define __BOARD_COM_API_H__

#include "tuya_cloud_types.h"
#include "sh3001.h"

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_st7735s.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Speaker amp enable: P7, active LOW */
#define BOARD_SPEAKER_EN_PIN   TUYA_GPIO_NUM_7

/* Onboard SH3001 IMU I2C: SCL=P42, SDA=P43 (soft I2C0; HW I2C0 only supports P20/P21) */
#define BOARD_SH3001_I2C_PORT  TUYA_I2C_NUM_0
#define BOARD_SH3001_SCL_PIN   TUYA_GPIO_NUM_42
#define BOARD_SH3001_SDA_PIN   TUYA_GPIO_NUM_43

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
/*
 * 0.96" ST7735S SPI TFT
 * Wiring (module -> T5AI):
 *   SCL(SCK) -> P14
 *   SDA(MOSI)-> P16   !!! T5AI HW SPI0 G0 requires MOSI=P16 when SCK=P14
 *                      (P15 is SPI0_CSN in the same group, cannot be MOSI)
 *   CS       -> P34
 *   DC       -> P36
 *   RES      -> P35
 *   BLK      -> P25
 *
 * Pinmux still maps the full SPI0 G0 group P14/P15/P16/P17; LCD CS is GPIO P34.
 */
#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_25
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH              80
#define BOARD_LCD_HEIGHT             160
/* 0.96" 80x160 IPS GRAM is typically 132x162; visible window often at (26,1) */
#define BOARD_LCD_X_OFFSET           26
#define BOARD_LCD_Y_OFFSET           1
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_270

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_CLK_PIN        TUYA_GPIO_NUM_14
#define BOARD_LCD_SPI_MOSI_PIN       TUYA_GPIO_NUM_16
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_34
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_36
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_35

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX
#define BOARD_LCD_POWER_ACTIVE_LV    TUYA_GPIO_LEVEL_HIGH
#endif

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Registers board peripherals (audio, button, ST7735S, SH3001 IMU).
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_hardware(void);

/**
 * @brief Get SH3001 device handle after successful board_register_hardware()
 * @return device pointer, or NULL if IMU was not initialized
 */
sh3001_dev_t *board_sh3001_get_dev(void);

/**
 * @brief Map SH3001 chip-frame accel to board frame (g)
 * @param[in] ax ay az chip-frame accel
 * @param[out] ax_b ay_b az_b board-frame accel
 * @return none
 * @note Measured when PCB is flat (screen up): chip gravity ≈ (-0.7, 0, -0.7).
 *       Rotate +45° about Y so flat becomes ≈ (0, 0, -1); then board +X is
 *       "screen vertical" with zero at horizontal.
 */
void board_sh3001_map_accel_to_board(float ax, float ay, float az,
                                     float *ax_b, float *ay_b, float *az_b);

/**
 * @brief Map SH3001 chip-frame gyro to board frame (dps)
 * @param[in] gx gy gz chip-frame gyro
 * @param[out] gx_b gy_b gz_b board-frame gyro
 * @return none
 */
void board_sh3001_map_gyro_to_board(float gx, float gy, float gz,
                                    float *gx_b, float *gy_b, float *gz_b);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_COM_API_H__ */

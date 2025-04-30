/**
 * @file board_config.h
 * @brief board_config module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "sdkconfig.h"
#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* Example configurations */
#define I2S_INPUT_SAMPLE_RATE     (16000)
#define I2S_OUTPUT_SAMPLE_RATE     (16000)

/* I2C port and GPIOs */
#define I2C_NUM         (0)
#define I2C_SCL_IO      (42)
#define I2C_SDA_IO      (41)

/* I2S port and GPIOs */
#define I2S_NUM         (0)
#define I2S_MCK_IO      (3)
#define I2S_BCK_IO      (46)
#define I2S_WS_IO       (9)

#define I2S_DO_IO       (10)
#define I2S_DI_IO       (14)

#define AUDIO_CODEC_DMA_DESC_NUM (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8388_ADDR (0x20)

/* XL9555 Extended IO */
#define SPK_EN_IO                   0x0004                          /* Speaker enable IO pin */
#define BEEP_IO                     0x0008
#define AP_INT_IO                   0x0001
#define QMA_INT_IO                  0x0002
#define OV_PWDN_IO                  0x0010
#define OV_RESET_IO                 0x0020
#define GBC_LED_IO                  0x0040
#define GBC_KEY_IO                  0x0080
#define LCD_BL_IO                   0x0100
#define CT_RST_IO                   0x0200
#define SLCD_RST_IO                 0x0400
#define SLCD_PWR_IO                 0x0800
#define KEY3_IO                     0x1000
#define KEY2_IO                     0x2000
#define KEY1_IO                     0x4000
#define KEY0_IO                     0x8000

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONFIG_H__ */

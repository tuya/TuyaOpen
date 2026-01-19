/**
 * @file tuya_t5ai_ex_module.h
 * @version 0.1
 * @date 2025-07-01
 */

#ifndef __TUYA_T5AI_EX_MODULE_H__
#define __TUYA_T5AI_EX_MODULE_H__

#include "tuya_cloud_types.h"

#if defined (TUYA_T5AI_BOARD_EX_MODULE_35565LCD) && (TUYA_T5AI_BOARD_EX_MODULE_35565LCD ==1)
#include "tdd_disp_ili9488.h"
#include "tdd_tp_gt1151.h"
#elif defined (TUYA_T5AI_BOARD_EX_MODULE_EYES) && (TUYA_T5AI_BOARD_EX_MODULE_EYES ==1)
#include "tdd_disp_st7735s.h"
#elif defined (TUYA_T5AI_BOARD_EX_MODULE_096_OLED) && (TUYA_T5AI_BOARD_EX_MODULE_096_OLED ==1)
#include "tdd_disp_ssd1306.h"
#endif

#if defined (ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA ==1)
#include "tdd_camera_gc2145.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if defined (TUYA_T5AI_BOARD_EX_MODULE_35565LCD) && (TUYA_T5AI_BOARD_EX_MODULE_35565LCD ==1)
#define BOARD_LCD_SW_SPI_CLK_PIN     TUYA_GPIO_NUM_49
#define BOARD_LCD_SW_SPI_CSX_PIN     TUYA_GPIO_NUM_48
#define BOARD_LCD_SW_SPI_SDA_PIN     TUYA_GPIO_NUM_50
#define BOARD_LCD_SW_SPI_DC_PIN      TUYA_GPIO_NUM_MAX
#define BOARD_LCD_SW_SPI_RST_PIN     TUYA_GPIO_NUM_53

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_PWM
#define BOARD_LCD_BL_PWM_ID          TUYA_PWM_NUM_7

#define BOARD_LCD_WIDTH              320
#define BOARD_LCD_HEIGHT             480
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_13
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_15

#elif defined (TUYA_T5AI_BOARD_EX_MODULE_EYES) && (TUYA_T5AI_BOARD_EX_MODULE_EYES ==1)
#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_25
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_WIDTH              128
#define BOARD_LCD_HEIGHT             128
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_180

// #define BOARD_LCD_SPI_PORT          TUYA_SPI_NUM_3
// #define BOARD_LCD_SPI_CLK           48000000
// #define BOARD_LCD_SPI_CS_PIN        TUYA_GPIO_NUM_3
// #define BOARD_LCD_SPI_DC_PIN        TUYA_GPIO_NUM_5
// #define BOARD_LCD_SPI_RST_PIN       TUYA_GPIO_NUM_45

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_2
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_23
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_7
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_6

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#if defined(ENABLE_EYES_TWO_LCD) && (ENABLE_EYES_TWO_LCD == 1)
#define BOARD_LCD_SPI2_PORT          TUYA_SPI_NUM_3
#define BOARD_LCD_SPI2_CLK           48000000
#define BOARD_LCD_SPI2_CS_PIN        TUYA_GPIO_NUM_3
#define BOARD_LCD_SPI2_DC_PIN        TUYA_GPIO_NUM_5
#define BOARD_LCD_SPI2_RST_PIN       TUYA_GPIO_NUM_45

// #define BOARD_LCD_SPI2_PORT           TUYA_SPI_NUM_2
// #define BOARD_LCD_SPI2_CLK            48000000
// #define BOARD_LCD_SPI2_CS_PIN         TUYA_GPIO_NUM_23
// #define BOARD_LCD_SPI2_DC_PIN         TUYA_GPIO_NUM_7
// #define BOARD_LCD_SPI2_RST_PIN        TUYA_GPIO_NUM_6
#endif

#elif defined (TUYA_T5AI_BOARD_EX_MODULE_096_OLED) && (TUYA_T5AI_BOARD_EX_MODULE_096_OLED ==1)
#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_NONE 

#define BOARD_LCD_WIDTH              128
#define BOARD_LCD_HEIGHT             64
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_COLOR_INVERSE      true
#define BOARD_LCD_COM_PIN_CFG        SSD1306_COM_PIN_CFG

#define BOARD_LCD_I2C_PORT           TUYA_I2C_NUM_0
#define BOARD_LCD_I2C_SLAVER_ADDR    SSD1306_I2C_ADDR

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX
#endif

#if defined (ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA ==1)
#define BOARD_CAMERA_I2C_PORT        TUYA_I2C_NUM_0
#define BOARD_CAMERA_I2C_SCL         TUYA_GPIO_NUM_13
#define BOARD_CAMERA_I2C_SDA         TUYA_GPIO_NUM_15

#define BOARD_CAMERA_RST_PIN         TUYA_GPIO_NUM_51
#define BOARD_CAMERA_RST_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_POWER_PIN       TUYA_GPIO_NUM_MAX

#define BOARD_CAMERA_CLK             24000000
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET board_register_ex_module(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_T5AI_EX_MODULE_H__ */

/**
 * @file board_ex_module.h
 * @version 0.1
 * @date 2025-07-01
 */

#ifndef __BOARD_EX_MODULE_H__
#define __BOARD_EX_MODULE_H__

#include "tuya_cloud_types.h"

#if (defined(ATK_T5AI_MINI_BOARD_LCD_MD0240_SPI) && (ATK_T5AI_MINI_BOARD_LCD_MD0240_SPI ==1))
#include "tdl_display_driver.h"
#include "tdd_disp_st7789.h"
#elif (defined(ATK_T5AI_MINI_BOARD_LCD_MD0700R_RGB) && (ATK_T5AI_MINI_BOARD_LCD_MD0700R_RGB ==1))
#include "tdl_display_driver.h"
#include "atk_t5ai_disp_md0700r.h"
#include "tdd_tp_gt911.h"
#elif (defined(ATK_T5AI_MINI_BOARD_LCD_MD0430R_RGB) && (ATK_T5AI_MINI_BOARD_LCD_MD0430R_RGB ==1))
#include "tdl_display_driver.h"
#include "atk_t5ai_disp_md0430r.h"
#include "tdd_tp_gt1151.h"
#endif

#if defined (ATK_T5AI_MINI_BOARD_CAMERA_OV2640) && (ATK_T5AI_MINI_BOARD_CAMERA_OV2640 ==1)
#include "tdd_camera_ov2640.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if (defined(ATK_T5AI_MINI_BOARD_LCD_MD0240_SPI) && (ATK_T5AI_MINI_BOARD_LCD_MD0240_SPI ==1))

#define BOARD_LCD_WIDTH              240
#define BOARD_LCD_HEIGHT             320
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_0
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_42
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_43
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_45
#define BOARD_LCD_SPI_SCL_PIN        TUYA_GPIO_NUM_44
#define BOARD_LCD_SPI_SDA_PIN        TUYA_GPIO_NUM_46
#define BOARD_LCD_SPI_SDI_PIN        TUYA_GPIO_NUM_47

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_9
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#elif (defined(ATK_T5AI_MINI_BOARD_LCD_MD0700R_RGB) && (ATK_T5AI_MINI_BOARD_LCD_MD0700R_RGB ==1))
#define BOARD_LCD_WIDTH              800
#define BOARD_LCD_HEIGHT             480
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_9
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_RST_PIN            TUYA_GPIO_NUM_27
#define BOARD_LCD_RST_ACTIVE_LV      TUYA_GPIO_LEVEL_LOW

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_13
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_15

#elif (defined(ATK_T5AI_MINI_BOARD_LCD_MD0430R_RGB) && (ATK_T5AI_MINI_BOARD_LCD_MD0430R_RGB ==1))
#define BOARD_LCD_WIDTH              800
#define BOARD_LCD_HEIGHT             480
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_0

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_9
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_RST_PIN            TUYA_GPIO_NUM_27
#define BOARD_LCD_RST_ACTIVE_LV      TUYA_GPIO_LEVEL_LOW

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_0
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_13
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_15

#endif

#if defined (ATK_T5AI_MINI_BOARD_CAMERA_OV2640) && (ATK_T5AI_MINI_BOARD_CAMERA_OV2640 ==1)
#define BOARD_CAMERA_I2C_PORT        TUYA_I2C_NUM_0
#define BOARD_CAMERA_I2C_SCL         TUYA_GPIO_NUM_13
#define BOARD_CAMERA_I2C_SDA         TUYA_GPIO_NUM_15

#define BOARD_CAMERA_RST_PIN         TUYA_GPIO_NUM_8
#define BOARD_CAMERA_RST_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_POWER_PIN       TUYA_GPIO_NUM_7
#define BOARD_CAMERA_PWR_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW

#define BOARD_CAMERA_CLK             0  //0 means use internal PLL as camera clock source
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

#endif /* __BOARD_EX_MODULE_H__ */

/**
 * @file board_ex_module.h
 * @version 0.1
 * @date 2025-07-01
 */

#ifndef __BOARD_EX_MODULE_H__
#define __BOARD_EX_MODULE_H__

#include "tuya_cloud_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if defined (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD) && (ATK_T5AI_MINI_BOARD_EX_MODULE_LCD ==1)
#define BOARD_GPIO_LCD_ID0           TUYA_GPIO_NUM_19
#define BOARD_GPIO_LCD_ID1           TUYA_GPIO_NUM_24
#define BOARD_GPIO_LCD_ID2           TUYA_GPIO_NUM_43

#define BOARD_LCD_RST_PIN            TUYA_GPIO_NUM_27

#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_GPIO 
#define BOARD_LCD_BL_PIN             TUYA_GPIO_NUM_9
#define BOARD_LCD_BL_ACTIVE_LV       TUYA_GPIO_LEVEL_HIGH

#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX

#define BOARD_TOUCH_I2C_PORT         TUYA_I2C_NUM_0
#define BOARD_TOUCH_I2C_SCL_PIN      TUYA_GPIO_NUM_13
#define BOARD_TOUCH_I2C_SDA_PIN      TUYA_GPIO_NUM_15

#define ATK_MD0430R_480272_ID        0x4342
#define ATK_MD0430R_480272_WIDTH     480
#define ATK_MD0430R_480272_HEIGHT    272
#define ATK_MD0430R_480272_ROTATION  TUYA_DISPLAY_ROTATION_0 
#define ATK_MD0430R_480272_FMT       TUYA_PIXEL_FMT_RGB565
#define ATK_MD0430R_480272_HSW       4
#define ATK_MD0430R_480272_HBP       43
#define ATK_MD0430R_480272_HFP       8
#define ATK_MD0430R_480272_VSW       4
#define ATK_MD0430R_480272_VBP       12
#define ATK_MD0430R_480272_VFP       8
#define ATK_MD0430R_480272_CLK      (10*1000000)
#define ATK_MD0430R_480272_CLK_EDGE TUYA_RGB_DATA_IN_RISING_EDGE

#define ATK_MD0430R_800480_ID        0x4384
#define ATK_MD0430R_800480_WIDTH     800
#define ATK_MD0430R_800480_HEIGHT    480
#define ATK_MD0430R_800480_ROTATION  TUYA_DISPLAY_ROTATION_0 
#define ATK_MD0430R_800480_FMT       TUYA_PIXEL_FMT_RGB565
#define ATK_MD0430R_800480_HSW       4//48
#define ATK_MD0430R_800480_HBP       4//88
#define ATK_MD0430R_800480_HFP       8//40
#define ATK_MD0430R_800480_VSW       4//3
#define ATK_MD0430R_800480_VBP       8//32
#define ATK_MD0430R_800480_VFP       8//13
#define ATK_MD0430R_800480_CLK      (26*1000000)
#define ATK_MD0430R_800480_CLK_EDGE  TUYA_RGB_DATA_IN_FALLING_EDGE
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

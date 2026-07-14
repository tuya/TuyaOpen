/**
 * @file lckfb_t5ai_ex_module.h
 * @version 0.1
 * @date 2025-07-01
 */

#ifndef __LCKFB_T5AI_EX_MODULE_H__
#define __LCKFB_T5AI_EX_MODULE_H__

#include "tuya_cloud_types.h"

#if defined (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320) && (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320 ==1)
#include "tdd_disp_st7789.h"
#include "tdd_tp_ft6336.h"
#endif

#if defined (LCKFB_T5AI_TOUCH_CAMERA) && (LCKFB_T5AI_TOUCH_CAMERA ==1)
#include "tdd_camera_gc0308.h"
#endif

#if defined(LCKFB_T5AI_TOUCH_SC7A20) && (LCKFB_T5AI_TOUCH_SC7A20 == 1)
#include "sc7a20.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if defined (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320) && (LCKFB_T5AI_TOUCH_LCD_ST7789_240X320 ==1)
/* LCD_BL=P25, LCD_PWR_CTL=P13, LCD_CS=P3, LCD_CLK=P2, LCD_MOSI=P4, LCD_RESET=P12
 * P5 is wired as LCD DC (schematic may label it LCD_MISO; ST7789 does not use MISO). */
/* P25 = PWMG0_PWM5 -> TUYA_PWM_NUM_8 (see T5AI peripheral mapping). */
#define BOARD_LCD_BL_TYPE            TUYA_DISP_BL_TP_PWM
#define BOARD_LCD_BL_PWM_ID          TUYA_PWM_NUM_8
#define BOARD_LCD_BL_PWM_FREQ        1000
#define BOARD_LCD_BL_PWM_CYCLE       10000

#define BOARD_LCD_WIDTH              240
#define BOARD_LCD_HEIGHT             320
#define BOARD_LCD_PIXELS_FMT         TUYA_PIXEL_FMT_RGB565
#define BOARD_LCD_ROTATION           TUYA_DISPLAY_ROTATION_270

/* SPI1 on T5AI maps CLK/MOSI/CS/MISO to P2/P4/P3/P5 (see tkl_spi.c). */
#define BOARD_LCD_SPI_PORT           TUYA_SPI_NUM_1
#define BOARD_LCD_SPI_CLK            48000000
#define BOARD_LCD_SPI_CLK_PIN        TUYA_GPIO_NUM_2
#define BOARD_LCD_SPI_MOSI_PIN       TUYA_GPIO_NUM_4
#define BOARD_LCD_SPI_CS_PIN         TUYA_GPIO_NUM_3
#define BOARD_LCD_SPI_DC_PIN         TUYA_GPIO_NUM_5
#define BOARD_LCD_SPI_RST_PIN        TUYA_GPIO_NUM_12

/* P13 LCD power is enabled in lckfb_t5ai_touch.c __board_power_init(); do not re-drive here. */
#define BOARD_LCD_POWER_PIN          TUYA_GPIO_NUM_MAX
#define BOARD_LCD_POWER_ACTIVE_LV    TUYA_GPIO_LEVEL_HIGH

#define BOARD_TP_I2C_PORT            TUYA_I2C_NUM_1
#define BOARD_TP_I2C_SCL_PIN         TUYA_GPIO_NUM_42
#define BOARD_TP_I2C_SDA_PIN         TUYA_GPIO_NUM_43
#define BOARD_TP_INT_PIN             TUYA_GPIO_NUM_45
#define BOARD_TP_RST_PIN             TUYA_GPIO_NUM_46

/* GC2145 DVP connector shares I2C1 with FT6336 on this board. */
#define BOARD_CAMERA_RST_PIN         TUYA_GPIO_NUM_41
#define BOARD_CAMERA_RST_ACTIVE_LV   TUYA_GPIO_LEVEL_LOW
/* GC0308 PWDN: LOW=normal operation, HIGH=power down */
#define BOARD_CAMERA_PWDN_PIN        TUYA_GPIO_NUM_40
#define BOARD_CAMERA_PWDN_ENABLE_LV  TUYA_GPIO_LEVEL_HIGH
#define BOARD_DVP_PWR_ON_LV          TUYA_GPIO_LEVEL_HIGH
#define BOARD_DVP_PWR_OFF_LV         TUYA_GPIO_LEVEL_LOW
#define BOARD_CAMERA_PWR_SETTLE_MS   100
#define BOARD_CAMERA_SENSOR_PWR_MS   50
#endif

#if defined (LCKFB_T5AI_TOUCH_CAMERA) && (LCKFB_T5AI_TOUCH_CAMERA ==1)
#define BOARD_CAMERA_I2C_PORT        TUYA_I2C_NUM_1
#define BOARD_CAMERA_I2C_SCL         TUYA_GPIO_NUM_42
#define BOARD_CAMERA_I2C_SDA         TUYA_GPIO_NUM_43

/* BOARD_CAMERA_RST_PIN / PWDN / DVP timing: see LCD block above */

#define BOARD_DVP_PWR_PIN              TUYA_GPIO_NUM_9

#define BOARD_CAMERA_POWER_PIN       TUYA_GPIO_NUM_MAX

#define BOARD_CAMERA_CLK             24000000
#endif

/* ---- SC7A20 IMU sensor ---- */
#if defined(LCKFB_T5AI_TOUCH_SC7A20) && (LCKFB_T5AI_TOUCH_SC7A20 == 1)
#define BOARD_SC7A20_I2C_PORT        TUYA_I2C_NUM_1
#define BOARD_SC7A20_I2C_SCL_PIN     TUYA_GPIO_NUM_42
#define BOARD_SC7A20_I2C_SDA_PIN     TUYA_GPIO_NUM_43
#define BOARD_SC7A20_INT1_PIN        TUYA_GPIO_NUM_44
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

#endif /* __LCKFB_T5AI_EX_MODULE_H__ */

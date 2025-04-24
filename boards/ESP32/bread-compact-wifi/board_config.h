/**
 * @file board_config.h
 * @brief board_config module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "tuya_cloud_types.h"

#include "display_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define OLED_I2C_SCL (42)
#define OLED_I2C_SDA (41)

#if defined(OLED_128x32)
#define OLED_WIDTH  (128)
#define OLED_HEIGHT (32)
#elif defined(OLED_128x64)
#define OLED_WIDTH  (128)
#define OLED_HEIGHT (64)
#else
#error "Please define OLED_128x32 or OLED_128x64"
#endif

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

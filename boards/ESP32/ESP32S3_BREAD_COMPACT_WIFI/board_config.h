/**
 * @file board_config.h
 * @brief board_config module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define OLED_I2C_PORT (0)

#define OLED_I2C_ADDR (0x3C)

#define OLED_I2C_SCL (42)
#define OLED_I2C_SDA (41)

#define OLED_WIDTH  (128)
#define OLED_HEIGHT (32)
/* display */
#define DISPLAY_TYPE_UNKNOWN      0
#define DISPLAY_TYPE_OLED_SSD1306 1
#define DISPLAY_TYPE_LCD_SH8601   2

#define BOARD_DISPLAY_TYPE DISPLAY_TYPE_OLED_SSD1306

#define DISPLAY_WIDTH  OLED_WIDTH
#define DISPLAY_HEIGHT OLED_HEIGHT

/* lvgl config */
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

#define DISPLAY_MONOCHROME true

/* rotation */
#define DISPLAY_SWAP_XY  false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true

#define DISPLAY_COLOR_FORMAT LV_COLOR_FORMAT_UNKNOWN

// Only one of DISPLAY_BUFF_SPIRAM and DISPLAY_BUFF_DMA can be selected
#define DISPLAY_BUFF_SPIRAM 0
#define DISPLAY_BUFF_DMA    1

#define DISPLAY_SWAP_BYTES 0

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

int board_display_init(void);

void *board_display_get_panel_io_handle(void);

void *board_display_get_panel_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CONFIG_H__ */

/**
 * @file oled_ssd1306.h
 * @brief oled_ssd1306 module is used to
 * @version 0.1
 * @date 2025-05-12
 */

#ifndef __OLED_SSD1306_H__
#define __OLED_SSD1306_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

int oled_ssd1306_init(void);

void *oled_ssd1306_get_panel_io_handle(void);

void *oled_ssd1306_get_panel_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_SSD1306_H__ */

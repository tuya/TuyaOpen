/**
 * @file lcd_sh8601.h
 * @brief lcd_sh8601 module is used to
 * @version 0.1
 * @date 2025-05-12
 */

#ifndef __LCD_SH8601_H__
#define __LCD_SH8601_H__

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

int lcd_sh8601_init(void);

void lcd_sh8601_set_backlight(uint8_t brightness);

void *lcd_sh8601_get_panel_io_handle(void);

void *lcd_sh8601_get_panel_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_SH8601_H__ */

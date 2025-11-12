/**
 * @file lcd_gc9a01_spi.h
 * @brief lcd_gc9a01_spi module is used to
 * @version 0.1
 * @date 2025-05-28
 */

#ifndef __LCD_GC9A01_SPI_H__
#define __LCD_GC9A01_SPI_H__

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

int lcd_gc9a01_spi_init(void);

void *lcd_gc9a01_spi_get_panel_io_handle(void);

void *lcd_gc9a01_spi_get_panel_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_GC9A01_SPI_H__ */

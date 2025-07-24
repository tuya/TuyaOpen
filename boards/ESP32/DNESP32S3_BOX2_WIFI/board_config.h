/**
 * @file board_config.h
 * @author Tuya Inc.
 * @brief The hardware configuration header file of the DNESP32S3_BOX2_WIFI board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* I2C port and GPIOs */
#define I2C_NUM    (0)
#define I2C_SCL_IO (47)
#define I2C_SDA_IO (48)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (38)
#define I2S_BCK_IO (40)
#define I2S_WS_IO  (42)
#define I2S_DO_IO  (41)
#define I2S_DI_IO  (39)

#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8389_ADDR   (0x10 << 1)

/* io expander start */
#define IO_EXPANDER_TYPE_UNKNOWN 0
#define IO_EXPANDER_TYPE_TCA9554 1
#define IO_EXPANDER_TYPE_XL9555  2

#define BOARD_IO_EXPANDER_TYPE IO_EXPANDER_TYPE_XL9555

#define IO_EXPANDER_XL9555_ADDR_000 (0x20)
#define IO_EXPANDER_XL9555_ADDR     IO_EXPANDER_XL9555_ADDR_000

#define EX_IO_SBU2     (0x0001 << 3)
#define EX_IO_SBU1     (0x0001 << 4)
#define EX_IO_KEY_L    (0x0001 << 5)
#define EX_IO_KEY_Q    (0x0001 << 6)
#define EX_IO_KEY_M    (0x0001 << 7)
#define EX_IO_USB_SEL  (0x0001 << 8)
#define EX_IO_SPK_EN   (0x0001 << 9)
#define EX_IO_SYS_POW  (0x0001 << 10)
#define EX_IO_VBUS_EN  (0x0001 << 11)
#define EX_IO_4G_EN    (0x0001 << 12)
#define EX_IO_3V3A_EN  (0x0001 << 13)
#define EX_IO_CHG_CTRL (0x0001 << 14)
#define EX_IO_CHRG     (0x0001 << 15)
/* io expander end */

/* display */
#define DISPLAY_TYPE_UNKNOWN        0
#define DISPLAY_TYPE_OLED_SSD1306   1
#define DISPLAY_TYPE_LCD_SH8601     2
#define DISPLAY_TYPE_LCD_ST7789_80  3
#define DISPLAY_TYPE_LCD_ST7789_SPI 4

#define BOARD_DISPLAY_TYPE DISPLAY_TYPE_LCD_ST7789_80

/* lcd */
#define LCD_I80_CS  (14)
#define LCD_I80_DC  (12)
#define LCD_I80_RD  (10)
#define LCD_I80_WR  (11)
#define LCD_I80_RST (-1)
#define LCD_I80_BL  (21)

#define LCD_I80_D0 (13)
#define LCD_I80_D1 (9)
#define LCD_I80_D2 (8)
#define LCD_I80_D3 (7)
#define LCD_I80_D4 (6)
#define LCD_I80_D5 (5)
#define LCD_I80_D6 (4)
#define LCD_I80_D7 (3)

#define DISPLAY_BACKLIGHT_PIN           LCD_I80_BL
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#define DISPLAY_WIDTH  (320)
#define DISPLAY_HEIGHT (240)

/* lvgl config */
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * 10)

#define DISPLAY_MONOCHROME false

/* rotation */
#define DISPLAY_SWAP_XY  true
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false

#define DISPLAY_COLOR_FORMAT LV_COLOR_FORMAT_RGB565

// Only one of DISPLAY_BUFF_SPIRAM and DISPLAY_BUFF_DMA can be selected
#define DISPLAY_BUFF_SPIRAM 0
#define DISPLAY_BUFF_DMA    1

#define DISPLAY_SWAP_BYTES 1

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
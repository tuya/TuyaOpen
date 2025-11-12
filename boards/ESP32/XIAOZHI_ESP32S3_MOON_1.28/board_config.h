/**
 * @file board_config.h
 * @brief board_config module is used to
 * @version 0.1
 * @date 2025-04-23
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
#define I2C_SCL_IO (13)
#define I2C_SDA_IO (12)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (7)
#define I2S_BCK_IO (2)
#define I2S_WS_IO  (6)

#define I2S_DO_IO (8)
#define I2S_DI_IO (4)
#define GPIO_OUTPUT_PA (10)

#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR   (0x30)



/* display */
#define DISPLAY_TYPE_UNKNOWN        0
#define DISPLAY_TYPE_OLED_SSD1306   1
#define DISPLAY_TYPE_LCD_SH8601     2
#define DISPLAY_TYPE_LCD_ST7789_80  3
#define DISPLAY_TYPE_LCD_ST7789_SPI 4
#define DISPLAY_TYPE_LCD_GC9A01_SPI 5

#define BOARD_DISPLAY_TYPE DISPLAY_TYPE_LCD_GC9A01_SPI

#define LCD_SCLK_PIN (16)
#define LCD_MOSI_PIN (17)
#define LCD_MISO_PIN (33) // Not used, can be set to GPIO_NUM_NC
#define LCD_DC_PIN   (14)
#define LCD_CS_PIN   (15)
#define LCD_RST_PIN  (18)

#define DISPLAY_BACKLIGHT_PIN           (21)
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#define DISPLAY_WIDTH  (240)
#define DISPLAY_HEIGHT (240)

/* lvgl config */
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * 20)

#define DISPLAY_MONOCHROME false

/* rotation */
#define DISPLAY_SWAP_XY  false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true

#define DISPLAY_COLOR_FORMAT LV_COLOR_FORMAT_RGB565

// Only one of DISPLAY_BUFF_SPIRAM and DISPLAY_BUFF_DMA can be selected
#define DISPLAY_BUFF_SPIRAM 0
#define DISPLAY_BUFF_DMA    1

#define DISPLAY_SWAP_BYTES 1

#define BOARD_BUTTON_PIN       (0)
#define BOARD_BUTTON_ACTIVE_LV TUYA_GPIO_LEVEL_LOW
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

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
/* display */
#define DISPLAY_TYPE_UNKNOWN      0
#define DISPLAY_TYPE_OLED_SSD1306 1
#define DISPLAY_TYPE_LCD_SH8601   2

#define BOARD_DISPLAY_TYPE DISPLAY_TYPE_LCD_SH8601

/* io expander */
#define IO_EXPANDER_TYPE_UNKNOWN 0
#define IO_EXPANDER_TYPE_TCA9554 1

#define BOARD_IO_EXPANDER_TYPE IO_EXPANDER_TYPE_TCA9554

/* Example configurations */
#define I2S_INPUT_SAMPLE_RATE  (16000)
#define I2S_OUTPUT_SAMPLE_RATE (16000)

/* I2C port and GPIOs */
#define I2C_NUM    (0)
#define I2C_SCL_IO (14)
#define I2C_SDA_IO (15)

/* I2S port and GPIOs */
#define I2S_NUM    (0)
#define I2S_MCK_IO (16)
#define I2S_BCK_IO (9)
#define I2S_WS_IO  (45)

#define I2S_DO_IO (8)
#define I2S_DI_IO (10)

#define GPIO_OUTPUT_PA (46)

#define AUDIO_CODEC_DMA_DESC_NUM  (6)
#define AUDIO_CODEC_DMA_FRAME_NUM (240)
#define AUDIO_CODEC_ES8311_ADDR   (0x30)

/* io expander */
#define IO_EXPANDER_TCA9554_ADDR_000 (0x20)
#define IO_EXPANDER_TCA9554_ADDR     IO_EXPANDER_TCA9554_ADDR_000

/* SPI */
#define SPI_NUM      (1)
#define SPI_SCLK_IO  (11)
#define SPI_MOSI_IO  (4)
#define SPI_MISO_IO  (5)
#define SPI_DATA2_IO (6)
#define SPI_DATA3_IO (7)

/* lcd */
#define SPI_CS_LCD (12)

#define DISPLAY_WIDTH  368
#define DISPLAY_HEIGHT 448

/* lvgl config */
#define DISPLAY_BUFFER_SIZE (DISPLAY_WIDTH * 10)

#define DISPLAY_MONOCHROME false

/* rotation */
#define DISPLAY_SWAP_XY  false
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false

#define DISPLAY_COLOR_FORMAT LV_COLOR_FORMAT_RGB565

#define DISPLAY_BUFF_DMA   1
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

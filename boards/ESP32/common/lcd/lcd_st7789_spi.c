/**
 * @file lcd_st7789_spi.c
 * @brief lcd_st7789_spi module is used to
 * @version 0.1
 * @date 2025-05-28
 */

#include "board_config.h"

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_ST7789_SPI)

#include "lcd_st7789_spi.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#include <driver/spi_common.h>
#include <driver/gpio.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "LCD_ST7789_SPI"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t panel;
} LCD_CONFIG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static LCD_CONFIG_T lcd_config = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static int __lcd_spi_init(void)
{
    esp_err_t esp_rt = ESP_OK;

    spi_bus_config_t buscfg = {0};
    buscfg.mosi_io_num = LCD_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = LCD_SCLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(esp_rt));
        return -1;
    }
    ESP_LOGD(TAG, "SPI bus initialized");

    return 0;
}

int lcd_st7789_spi_init(void)
{
    if (__lcd_spi_init() != 0) {
        return -1;
    }

    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = LCD_CS_PIN;
    io_config.dc_gpio_num = LCD_DC_PIN;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 7;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &lcd_config.panel_io);

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    esp_lcd_new_panel_st7789(lcd_config.panel_io, &panel_config, &lcd_config.panel);

    esp_lcd_panel_reset(lcd_config.panel);

    esp_lcd_panel_init(lcd_config.panel);
    esp_lcd_panel_invert_color(lcd_config.panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    esp_lcd_panel_swap_xy(lcd_config.panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(lcd_config.panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    return 0;
}

void *lcd_st7789_spi_get_panel_io_handle(void)
{
    return lcd_config.panel_io;
}

void *lcd_st7789_spi_get_panel_handle(void)
{
    return lcd_config.panel;
}

#endif // BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_ST7789_SPI

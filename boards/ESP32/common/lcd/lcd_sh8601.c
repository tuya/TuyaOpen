/**
 * @file lcd_sh8601.c
 * @brief lcd_sh8601 module is used to
 * @version 0.1
 * @date 2025-05-12
 */

#include "board_config.h"

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_SH8601)

#include "lcd_sh8601.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_lvgl_port.h"

#include <driver/spi_master.h>
#include "driver/gpio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "LCD_SH8601"

#define LCD_OPCODE_WRITE_CMD   (0x02ULL)
#define LCD_OPCODE_READ_CMD    (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

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
static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {{0x11, (uint8_t[]){0x00}, 0, 120},
                                                             {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
                                                             {0x35, (uint8_t[]){0x00}, 1, 0},
                                                             {0x53, (uint8_t[]){0x20}, 1, 10},
                                                             {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
                                                             {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
                                                             {0x51, (uint8_t[]){0x00}, 1, 10},
                                                             {0x29, (uint8_t[]){0x00}, 0, 10}};

static LCD_CONFIG_T lcd_config = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/* lcd spi init */
static int __lcd_spi_init(void)
{
    esp_err_t esp_rt = ESP_OK;

    spi_bus_config_t buscfg = {0};
    buscfg.sclk_io_num = SPI_SCLK_IO;
    buscfg.data0_io_num = SPI_MOSI_IO;
    buscfg.data1_io_num = SPI_MISO_IO;
    buscfg.data2_io_num = SPI_DATA2_IO;
    buscfg.data3_io_num = SPI_DATA3_IO;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    buscfg.flags = SPICOMMON_BUSFLAG_QUAD;

    esp_rt = spi_bus_initialize(SPI_NUM, &buscfg, SPI_DMA_CH_AUTO);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    return 0;
}

int lcd_sh8601_init(void)
{
    esp_err_t esp_rt = ESP_OK;
    int rt = 0;

    rt = __lcd_spi_init();
    if (rt != 0) {
        return -1;
    }

    esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(SPI_CS_LCD, NULL, NULL);
    esp_rt = esp_lcd_new_panel_io_spi(SPI_NUM, &io_config, &lcd_config.panel_io);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel io: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    const sh8601_vendor_config_t vendor_config = {.init_cmds = &vendor_specific_init[0],
                                                  .init_cmds_size =
                                                      sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
                                                  .flags = {
                                                      .use_qspi_interface = 1,
                                                  }};
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_NC;
    panel_config.flags.reset_active_high = 1, panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = (void *)&vendor_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(lcd_config.panel_io, &panel_config, &lcd_config.panel));

    esp_lcd_panel_reset(lcd_config.panel);
    esp_lcd_panel_init(lcd_config.panel);
    esp_lcd_panel_invert_color(lcd_config.panel, false);
    esp_lcd_panel_mirror(lcd_config.panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    return 0;
}

void *lcd_sh8601_get_panel_io_handle(void)
{
    return lcd_config.panel_io;
}

void *lcd_sh8601_get_panel_handle(void)
{
    return lcd_config.panel;
}

void lcd_sh8601_set_backlight(uint8_t brightness)
{
    if (lcd_config.panel_io == NULL) {
        return;
    }

    uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
    int lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    esp_lcd_panel_io_tx_param(lcd_config.panel_io, lcd_cmd, &data, sizeof(data));
}

#endif /* BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_SH8601 */

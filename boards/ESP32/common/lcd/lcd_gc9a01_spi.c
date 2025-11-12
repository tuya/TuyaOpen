/**
 * @file lcd_gc9a01_spi.c
 * @brief lcd_gc9a01_spi module is used to
 * @version 0.1
 * @date 2025-05-28
 */

#include "board_config.h"

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_GC9A01_SPI)

#include "lcd_gc9a01_spi.h"
#include "esp_lcd_gc9a01.h"
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
#define TAG "LCD_GC9A01_SPI"

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
void init_backlight_simple(void)
{
    // 直接重置GPIO配置
    gpio_reset_pin(DISPLAY_BACKLIGHT_PIN);
    
    // 设置为输出模式
    gpio_set_direction(DISPLAY_BACKLIGHT_PIN, GPIO_MODE_OUTPUT);
    
    // 输出高电平
    gpio_set_level(DISPLAY_BACKLIGHT_PIN, 1);
}

int lcd_gc9a01_spi_init(void)
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

    ESP_LOGD(TAG, "Install GC9A01 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = LCD_RST_PIN;
    panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           //LCD_RGB_ENDIAN_RGB;
    panel_config.bits_per_pixel = 16;
    // panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    esp_lcd_new_panel_gc9a01(lcd_config.panel_io, &panel_config, &lcd_config.panel);

    esp_lcd_panel_reset(lcd_config.panel);

    esp_lcd_panel_init(lcd_config.panel);
    esp_lcd_panel_invert_color(lcd_config.panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    esp_lcd_panel_swap_xy(lcd_config.panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(lcd_config.panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    esp_lcd_panel_disp_on_off(lcd_config.panel, true);

    uint8_t data_0x62[] = { 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70 };
    esp_lcd_panel_io_tx_param(lcd_config.panel_io, 0x62, data_0x62, sizeof(data_0x62));

    uint8_t data_0x63[] = { 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70 };
    esp_lcd_panel_io_tx_param(lcd_config.panel_io, 0x63, data_0x63, sizeof(data_0x63));

    init_backlight_simple();

    return 0;
}

void *lcd_gc9a01_spi_get_panel_io_handle(void)
{
    return lcd_config.panel_io;
}

void *lcd_gc9a01_spi_get_panel_handle(void)
{
    return lcd_config.panel;
}

#endif // BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_GC9A01_SPI

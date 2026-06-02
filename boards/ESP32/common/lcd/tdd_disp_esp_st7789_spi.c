/**
 * @file tdd_disp_esp_st7789_spi.c
 * @brief ST7789 SPI LCD register helper.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_st7789_spi.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "tdd_disp_esp_st7789_spi"

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t    panel;
} LCD_CONFIG_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static LCD_CONFIG_T sg_lcd_config = {0};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise the SPI bus for the ST7789 panel.
 * @param[in] hw     Hardware configuration.
 * @param[in] width  Display width.
 * @param[in] height Display height.
 * @return 0 on success, -1 on failure.
 */
static int __lcd_spi_init(LCD_ST7789_SPI_HW_CFG_T *hw, uint16_t width, uint16_t height)
{
    esp_err_t        esp_rt = ESP_OK;
    spi_bus_config_t buscfg = {0};

    buscfg.mosi_io_num     = hw->mosi_io;
    buscfg.miso_io_num     = GPIO_NUM_NC;
    buscfg.sclk_io_num     = hw->sclk_io;
    buscfg.quadwp_io_num   = GPIO_NUM_NC;
    buscfg.quadhd_io_num   = GPIO_NUM_NC;
    buscfg.max_transfer_sz = (int)width * (int)height * sizeof(uint16_t);

    esp_rt = spi_bus_initialize(hw->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(esp_rt));
        return -1;
    }
    ESP_LOGD(TAG, "SPI bus initialized");

    return 0;
}

/**
 * @brief Bring up the ST7789 SPI panel.
 * @param[in] hw     Hardware configuration.
 * @param[in] width  Display width.
 * @param[in] height Display height.
 * @return 0 on success, -1 on failure.
 */
static int __lcd_st7789_spi_init(LCD_ST7789_SPI_HW_CFG_T *hw, uint16_t width, uint16_t height)
{
    if (__lcd_spi_init(hw, width, height) != 0) {
        return -1;
    }

    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {0};
    io_config.cs_gpio_num                   = hw->cs_io;
    io_config.dc_gpio_num                   = hw->dc_io;
    io_config.spi_mode                      = 0;
    io_config.pclk_hz                       = 40 * 1000 * 1000;
    io_config.trans_queue_depth             = 7;
    io_config.lcd_cmd_bits                  = 8;
    io_config.lcd_param_bits                = 8;
    esp_lcd_new_panel_io_spi(hw->spi_host, &io_config, &sg_lcd_config.panel_io);

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {0};
    panel_config.reset_gpio_num             = (hw->rst_io < 0) ? GPIO_NUM_NC : hw->rst_io;
    panel_config.rgb_ele_order              = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel             = 16;
    panel_config.data_endian                = LCD_RGB_DATA_ENDIAN_BIG;
    esp_lcd_new_panel_st7789(sg_lcd_config.panel_io, &panel_config, &sg_lcd_config.panel);

    esp_lcd_panel_reset(sg_lcd_config.panel);
    esp_lcd_panel_init(sg_lcd_config.panel);
    esp_lcd_panel_invert_color(sg_lcd_config.panel, hw->invert_color);
    esp_lcd_panel_swap_xy(sg_lcd_config.panel, hw->swap_xy);
    esp_lcd_panel_mirror(sg_lcd_config.panel, hw->mirror_x, hw->mirror_y);

    return 0;
}

OPERATE_RET tdd_disp_esp_st7789_spi_register(char *name, LCD_ST7789_SPI_HW_CFG_T *hw, TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (__lcd_st7789_spi_init(hw, cfg->width, cfg->height) != 0) {
        return OPRT_COM_ERROR;
    }

    return tdd_disp_esp_lcd_register(name, sg_lcd_config.panel_io, sg_lcd_config.panel, cfg);
}

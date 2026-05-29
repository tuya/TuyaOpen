/**
 * @file tdd_disp_esp_sh8601.c
 * @brief SH8601 QSPI LCD register helper.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_sh8601.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"

#include <driver/spi_master.h>
#include "driver/gpio.h"

#include "tdl_display_manage.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "tdd_disp_esp_sh8601"

#define LCD_OPCODE_WRITE_CMD   (0x02ULL)
#define LCD_OPCODE_READ_CMD    (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

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
static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00},                   0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1},             2, 0  },
    {0x35, (uint8_t[]){0x00},                   1, 0  },
    {0x53, (uint8_t[]){0x20},                   1, 10 },
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0  },
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0  },
    {0x51, (uint8_t[]){0x00},                   1, 10 },
    {0x29, (uint8_t[]){0x00},                   0, 10 },
};

static LCD_CONFIG_T sg_lcd_config = {0};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise the QSPI bus for the SH8601 panel.
 * @param[in] hw    Hardware configuration.
 * @param[in] width Display width in pixels (used to size the DMA transfer).
 * @param[in] height Display height in pixels.
 * @return 0 on success, -1 on failure.
 */
static int __lcd_spi_init(LCD_SH8601_HW_CFG_T *hw, uint16_t width, uint16_t height)
{
    esp_err_t        esp_rt = ESP_OK;
    spi_bus_config_t buscfg = {0};

    buscfg.sclk_io_num     = hw->sclk_io;
    buscfg.data0_io_num    = hw->data0_io;
    buscfg.data1_io_num    = hw->data1_io;
    buscfg.data2_io_num    = hw->data2_io;
    buscfg.data3_io_num    = hw->data3_io;
    buscfg.max_transfer_sz = (int)width * (int)height * sizeof(uint16_t);
    buscfg.flags           = SPICOMMON_BUSFLAG_QUAD;

    esp_rt = spi_bus_initialize(hw->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    return 0;
}

/**
 * @brief Bring up the SH8601 panel and stash the resulting esp_lcd handles.
 * @param[in] hw     Hardware configuration.
 * @param[in] width  Display width.
 * @param[in] height Display height.
 * @return 0 on success, -1 on failure.
 */
static int __lcd_sh8601_init(LCD_SH8601_HW_CFG_T *hw, uint16_t width, uint16_t height)
{
    esp_err_t esp_rt = ESP_OK;

    if (__lcd_spi_init(hw, width, height) != 0) {
        return -1;
    }

    esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(hw->cs_io, NULL, NULL);
    esp_rt = esp_lcd_new_panel_io_spi(hw->spi_host, &io_config, &sg_lcd_config.panel_io);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel io: %s", esp_err_to_name(esp_rt));
        return -1;
    }

    const sh8601_vendor_config_t vendor_config = {
        .init_cmds      = &vendor_specific_init[0],
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
        .flags =
            {
                .use_qspi_interface = 1,
            },
    };
    esp_lcd_panel_dev_config_t panel_config = {0};
    panel_config.reset_gpio_num             = GPIO_NUM_NC;
    panel_config.flags.reset_active_high    = 1;
    panel_config.rgb_ele_order              = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel             = 16;
    panel_config.vendor_config              = (void *)&vendor_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(sg_lcd_config.panel_io, &panel_config, &sg_lcd_config.panel));

    esp_lcd_panel_reset(sg_lcd_config.panel);
    esp_lcd_panel_init(sg_lcd_config.panel);
    esp_lcd_panel_invert_color(sg_lcd_config.panel, false);
    esp_lcd_panel_mirror(sg_lcd_config.panel, hw->mirror_x, hw->mirror_y);

    return 0;
}

/**
 * @brief Push a backlight brightness (0..100) to the SH8601 via register 0x51.
 * @param[in] brightness 0..100 percent.
 * @return none
 */
static void __sh8601_set_backlight(uint8_t brightness)
{
    if (sg_lcd_config.panel_io == NULL) {
        return;
    }

    uint8_t data[1]  = {((uint8_t)((255 * brightness) / 100))};
    int     lcd_cmd  = 0x51;
    lcd_cmd         &= 0xff;
    lcd_cmd        <<= 8;
    lcd_cmd         |= LCD_OPCODE_WRITE_CMD << 24;
    esp_lcd_panel_io_tx_param(sg_lcd_config.panel_io, lcd_cmd, &data, sizeof(data));
}

/**
 * @brief Backlight callback registered with the TDL display layer.
 * @param[in] brightness 0..100 percent.
 * @param[in] arg        Unused.
 * @return OPRT_OK
 */
static OPERATE_RET __sh8601_backlight_cb(uint8_t brightness, void *arg)
{
    (void)arg;
    __sh8601_set_backlight(brightness);
    return OPRT_OK;
}

OPERATE_RET tdd_disp_esp_sh8601_register(char *name, LCD_SH8601_HW_CFG_T *hw, TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (__lcd_sh8601_init(hw, cfg->width, cfg->height) != 0) {
        return OPRT_COM_ERROR;
    }

    TDD_DISP_ESP_LCD_CFG_T local_cfg = *cfg;
    local_cfg.bl.type                = TUYA_DISP_BL_TP_CUSTOM;

    rt = tdd_disp_esp_lcd_register(name, sg_lcd_config.panel_io, sg_lcd_config.panel, &local_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    return tdl_disp_custom_backlight_register(name, __sh8601_backlight_cb, NULL);
}

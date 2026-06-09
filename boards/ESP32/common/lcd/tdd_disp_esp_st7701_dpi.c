/**
 * @file tdd_disp_esp_st7701_dpi.c
 * @brief ST7701 MIPI DSI DPI LCD driver for ESP32-P4.
 *
 * Uses the ESP-IDF esp_lcd_st7701 managed component (esp_lcd_new_panel_st7701)
 * which correctly wraps the DPI panel with ST7701-aware hooks for reset, init,
 * disp_on_off, mirror, invert_color, and sleep.
 *
 * Hardware bring-up sequence:
 *   1. Hardware reset pulse (RST GPIO)
 *   2. Enable MIPI DSI PHY power via LDO regulator (ESP32-P4)
 *   3. Create MIPI DSI bus (esp_lcd_new_dsi_bus)
 *   4. Create DBI command IO (esp_lcd_new_panel_io_dbi)
 *   5. Create ST7701 panel (esp_lcd_new_panel_st7701)
 *   6. esp_lcd_panel_reset() → esp_lcd_panel_init() → esp_lcd_panel_disp_on_off(true)
 *   7. Register with TuyaOpen TDD display framework
 *
 * Init command table sourced from the Waveshare BSP for
 * ESP32-P4-WIFI6-Touch-LCD-4.3 (ST7701/JD9365, 480x800, 2-lane MIPI DSI).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_st7701_dpi.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7701.h"
#include "esp_ldo_regulator.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define TAG "tdd_disp_esp_st7701_dpi"

/* ---------------------------------------------------------------------------
 * Waveshare 4.3" 480x800 ST7701 init command table.
 *
 * Verified-working sequence from the official Waveshare demo for
 * ESP32-P4-WIFI6-Touch-LCD-4.3:
 *   https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-4.3
 *
 * Critical vs a generic ST7701 480x480 table:
 *   - 0xC0 line setting = {0x63,0x00} => (0x63+1)*8 = 800 active lines
 *     (a 0x3B value would clamp the panel to 480 lines and leave a black band).
 *   - Full GIP gate-driver timing block (0xE0..0xED); omitting it produces
 *     fixed black lines and uneven brightness.
 *
 * Pixel format (0x3A) is left to the esp_lcd_st7701 component, which sets
 * COLMOD from panel_config.bits_per_pixel (16 => RGB565 / 0x55).
 * --------------------------------------------------------------------------- */
static const st7701_lcd_init_cmd_t vendor_specific_init[] = {
//  {cmd, data, data_bytes, delay_ms}
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x17, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x40, 0xC9, 0x94, 0x0E, 0x10, 0x05, 0x0B, 0x09,
            0x08, 0x26, 0x04, 0x52, 0x10, 0x69, 0x6B, 0x69}, 16, 0},
    {0xB1, (uint8_t[]){0x40, 0xD2, 0x98, 0x0C, 0x92, 0x07, 0x09, 0x08,
            0x07, 0x25, 0x02, 0x0E, 0x0C, 0x6E, 0x78, 0x55}, 16, 0},

    /* Page 1: power / internal timing */
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x4E}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},

    /* GIP gate-driver timing */
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0,
            0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0,
            0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0,
            0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0,
            0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF,
            0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},

    /* Back to page 0 */
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},

    /* Sleep Out (120 ms) then Display On */
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

/* MIPI DPI panel config for 480x800 @ ~60Hz with 30 MHz DPI clock.
 *
 * From Waveshare BSP display.h:
 *   H=480, V=800, HSYNC=12, HBP=42, HFP=42, VSYNC=8, VBP=2, VFP=60
 *   DPI clock: 30 MHz, lane bitrate: 500 Mbps
 *
 * Refresh rate ≈ 30_000_000 / ((480+12+42+42) * (800+8+2+60))
 *              = 30_000_000 / (576 * 870)
 *              ≈ 60 Hz
 */
static const esp_lcd_dpi_panel_config_t dpi_config = {
    .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 30,
    .virtual_channel    = 0,
    /* Match the official Waveshare BSP exactly: use .pixel_format (not
     * .in_color_format, which is a different color_hal enum) and a single
     * frame buffer. Using in_color_format configured the DSI bridge pixel
     * packing differently and produced faint vertical streaks on light fills. */
    .pixel_format       = LCD_COLOR_PIXEL_FORMAT_RGB565,
    .num_fbs            = 1,
    .video_timing = {
        .h_size            = 480,
        .v_size            = 800,
        .hsync_pulse_width = 12,
        .hsync_back_porch  = 42,
        .hsync_front_porch = 42,
        .vsync_pulse_width = 8,
        .vsync_back_porch  = 2,
        .vsync_front_porch = 60,
    },
    .flags = {
        /* Hardware-accelerated (2D-DMA / esp_async_fbcpy) copy of the draw buffer
         * into the internal frame buffer. Note: if the PPA (ENABLE_DMA2D) is ever
         * used concurrently it shares the same ESP32-P4 2D-DMA peripheral. */
        .use_dma2d  = 1,
        .disable_lp = 0,
    },
};

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    esp_lcd_dsi_bus_handle_t  mipi_dsi_bus;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_panel_handle_t    panel;
    esp_ldo_channel_handle_t  ldo_phy_chan;
} LCD_CONFIG_T;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static LCD_CONFIG_T sg_lcd_config = {0};

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

/**
 * @brief Enable MIPI DSI PHY power via LDO regulator.
 *
 * On ESP32-P4, the MIPI DSI PHY needs an external LDO supply.
 * The Waveshare board connects LDO_VO3 to VDD_MIPI_DPHY.
 *
 * @param[in] ldo_chan   LDO channel number.
 * @param[in] voltage_mv LDO output voltage in mV.
 * @return 0 on success, -1 on failure.
 */
static int __dsi_phy_power_on(int ldo_chan, int voltage_mv)
{
    if (ldo_chan < 0) {
        /* PHY power managed externally */
        return 0;
    }

    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = ldo_chan,
        .voltage_mv = voltage_mv,
    };
    esp_err_t err = esp_ldo_acquire_channel(&ldo_cfg, &sg_lcd_config.ldo_phy_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ldo_acquire_channel(chan=%d, mv=%d) failed: 0x%x",
                 ldo_chan, voltage_mv, err);
        return -1;
    }

    ESP_LOGI(TAG, "MIPI DSI PHY power ON (LDO chan %d, %d mV)", ldo_chan, voltage_mv);
    return 0;
}

/**
 * @brief Disable MIPI DSI PHY power.
 */
static void __dsi_phy_power_off(void)
{
    if (sg_lcd_config.ldo_phy_chan) {
        esp_ldo_release_channel(sg_lcd_config.ldo_phy_chan);
        sg_lcd_config.ldo_phy_chan = NULL;
        ESP_LOGI(TAG, "MIPI DSI PHY power OFF");
    }
}

/**
 * @brief Create the MIPI DSI bus.
 *
 * @return 0 on success, -1 on failure.
 */
static int __mipi_dsi_bus_init(void)
{
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id             = 0,
        .num_data_lanes     = 2,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 500,
    };

    esp_err_t esp_rt = esp_lcd_new_dsi_bus(&bus_config, &sg_lcd_config.mipi_dsi_bus);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_dsi_bus failed: 0x%x", esp_rt);
        return -1;
    }

    ESP_LOGI(TAG, "MIPI DSI bus created (2 lanes, 500 Mbps)");
    return 0;
}

/**
 * @brief Create the DBI command IO interface.
 *
 * @return 0 on success, -1 on failure.
 */
static int __dbi_io_init(void)
{
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };

    esp_err_t esp_rt = esp_lcd_new_panel_io_dbi(sg_lcd_config.mipi_dsi_bus,
                                                 &dbi_config,
                                                 &sg_lcd_config.panel_io);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_dbi failed: 0x%x", esp_rt);
        return -1;
    }

    ESP_LOGI(TAG, "MIPI DSI DBI command IO created");
    return 0;
}

/**
 * @brief Create the ST7701 panel via esp_lcd_new_panel_st7701().
 *
 * Uses the ESP-IDF managed component which internally creates a DPI panel
 * and wraps it with ST7701-aware hooks (reset, init, disp_on_off, mirror,
 * invert_color, sleep).
 *
 * @param[in] rst_io Reset GPIO number.
 * @return 0 on success, -1 on failure.
 */
static int __st7701_panel_init(int rst_io)
{
    st7701_vendor_config_t vendor_config = {
        .init_cmds      = vendor_specific_init,
        .init_cmds_size = sizeof(vendor_specific_init) / sizeof(st7701_lcd_init_cmd_t),
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus    = sg_lcd_config.mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num         = rst_io,
        .rgb_ele_order          = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel         = 16, /* RGB565 */
        .vendor_config          = &vendor_config,
    };

    esp_err_t esp_rt = esp_lcd_new_panel_st7701(sg_lcd_config.panel_io,
                                                  &panel_config,
                                                  &sg_lcd_config.panel);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7701 failed: 0x%x", esp_rt);
        return -1;
    }

    ESP_LOGI(TAG, "ST7701 panel created");
    return 0;
}

/**
 * @brief Cleanup all MIPI DSI resources on error.
 */
static void __cleanup(void)
{
    if (sg_lcd_config.panel) {
        esp_lcd_panel_del(sg_lcd_config.panel);
        sg_lcd_config.panel = NULL;
    }
    if (sg_lcd_config.panel_io) {
        esp_lcd_panel_io_del(sg_lcd_config.panel_io);
        sg_lcd_config.panel_io = NULL;
    }
    if (sg_lcd_config.mipi_dsi_bus) {
        esp_lcd_del_dsi_bus(sg_lcd_config.mipi_dsi_bus);
        sg_lcd_config.mipi_dsi_bus = NULL;
    }
    __dsi_phy_power_off();
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

/**
 * @brief Full ST7701 MIPI DSI DPI panel bring-up.
 *
 * Sequence (following ESP-IDF ST7701 MIPI test app pattern):
 *   1. Enable MIPI DSI PHY power (LDO)
 *   2. Create MIPI DSI bus
 *   3. Create DBI command IO
 *   4. Create ST7701 panel (esp_lcd_new_panel_st7701 wraps DPI panel)
 *   5. esp_lcd_panel_reset() → esp_lcd_panel_init() → esp_lcd_panel_disp_on_off(true)
 *
 * @param[in] hw     Hardware configuration.
 * @return 0 on success, -1 on failure.
 */
static int __lcd_st7701_dpi_init(LCD_ST7701_DPI_HW_CFG_T *hw)
{
    /* Step 1: Enable MIPI DSI PHY power */
    if (__dsi_phy_power_on(hw->phy_pwr_ldo_chan, hw->phy_pwr_ldo_mv) != 0) {
        return -1;
    }

    /* Step 2: Create MIPI DSI bus */
    if (__mipi_dsi_bus_init() != 0) {
        __dsi_phy_power_off();
        return -1;
    }

    /* Step 3: Create DBI command IO */
    if (__dbi_io_init() != 0) {
        __cleanup();
        return -1;
    }

    /* Step 4: Create ST7701 panel (internally creates DPI panel) */
    if (__st7701_panel_init(hw->rst_io) != 0) {
        __cleanup();
        return -1;
    }

    /* Step 5: Reset → Init → Display On */
    esp_lcd_panel_reset(sg_lcd_config.panel);
    esp_lcd_panel_init(sg_lcd_config.panel);
    esp_lcd_panel_disp_on_off(sg_lcd_config.panel, true);

    return 0;
}

OPERATE_RET tdd_disp_esp_st7701_dpi_register(char *name,
                                              LCD_ST7701_DPI_HW_CFG_T *hw,
                                              TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (__lcd_st7701_dpi_init(hw) != 0) {
        return OPRT_COM_ERROR;
    }

    return tdd_disp_esp_dpi_register(name,
                                     sg_lcd_config.panel_io,
                                     sg_lcd_config.panel,
                                     cfg);
}

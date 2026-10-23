/**
 * @file tdd_disp_spi_ssd2677.c
 * @brief SSD2677ZB full/partial-refresh driver for the SEEKINK E0397A09-A0 panel.
 *
 * Uses hardware SPI for commands and image data, with GPIO SDA temperature reads.
 * The panel-specific OTP settings and 1bpp-to-2bpp encoding follow the supplied
 * E0397A09-A0 vendor reference, validated on T5AI at 800 x 480 pixels.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tdd_disp_ssd2677.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tkl_gpio.h"
#include "tkl_spi.h"
#include "tdl_display_manage.h"
#include <string.h>

/* Command names follow the SSD2677 Rev 1.0 command table.
 * Keep opcodes private: applications configure the device through TDD. */
#define SSD2677_CMD_PANEL_SETTING               0x00 /* PSR */
#define SSD2677_CMD_POWER_OFF                   0x02 /* POF */
#define SSD2677_CMD_POWER_ON                    0x04 /* PON */
#define SSD2677_CMD_BOOSTER_SOFT_START          0x06 /* BTST */
#define SSD2677_CMD_DEEP_SLEEP                  0x07 /* DSLP */
#define SSD2677_CMD_DATA_START_TRANSMISSION     0x10 /* DTM */
#define SSD2677_CMD_DISPLAY_REFRESH             0x12 /* DRF */
#define SSD2677_CMD_FRAME_RATE_CONTROL          0x30 /* PLL */
#define SSD2677_CMD_TEMPERATURE_SENSOR          0x40 /* TSC */
#define SSD2677_CMD_VCOM_DATA_INTERVAL          0x50 /* CDI */
#define SSD2677_CMD_RESOLUTION_SETTING          0x61 /* TRES */
#define SSD2677_CMD_PARTIAL_WINDOW              0x83 /* PTLW */
#define SSD2677_CMD_GATE_SOURCE_START           0x65 /* GSST */
#define SSD2677_CMD_CASCADE_TEMPERATURE_SETTING 0xE0 /* CCSET */
#define SSD2677_CMD_TEMPERATURE_OVERRIDE        0xE6 /* TS_SET */

/* Panel-vendor extensions absent from the public Rev 1.0 command table.
 * HTOTAL is the vendor sample name; E7/E9 meanings are not documented.
 * A5 is used to load the temperature-selected waveform in that sample. */
#define SSD2677_CMD_VENDOR_HTOTAL        0x62
#define SSD2677_CMD_VENDOR_LOAD_WAVEFORM 0xA5
#define SSD2677_CMD_VENDOR_E7            0xE7
#define SSD2677_CMD_VENDOR_E9            0xE9

#define SSD2677_SOURCE_MAX 960
#define SSD2677_GATE_MIN   420
#define SSD2677_GATE_MAX   680

typedef struct {
    DISP_EINK_SSD2677_CFG_T cfg;
    uint16_t                scan_height;
    OPERATE_RET             io_result;
    MUTEX_HANDLE            mutex;
    bool                    opened;
    bool                    frame_ready;
    bool                    spi_initialized;
    uint8_t                 initialized_pins;
} DISP_SSD2677_DEV_T;

/** @brief Preserve the first GPIO write error and stop clocking after failure. */
static void pin_write(DISP_SSD2677_DEV_T *dev, TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E level)
{
    if (dev->io_result != OPRT_OK) {
        return;
    }
    OPERATE_RET rt = tkl_gpio_write(pin, level);
    if (rt != OPRT_OK && dev->io_result == OPRT_OK) {
        dev->io_result = rt;
    }
}

/** @brief Send synchronously through the hardware SPI controller. */
static void send_bytes(DISP_SSD2677_DEV_T *dev, const uint8_t *data, size_t len)
{
    if (dev->io_result == OPRT_OK && len) {
        dev->io_result = tkl_spi_send(dev->cfg.port, (void *)data, len);
    }
}

/** @brief Send one command/data byte through hardware SPI. */
static void send_byte(DISP_SSD2677_DEV_T *dev, uint8_t byte)
{
    send_bytes(dev, &byte, 1);
}

/** @brief Start a command and hold CS through its data or read phase. */
static void command(DISP_SSD2677_DEV_T *dev, uint8_t cmd, const uint8_t *data, size_t len)
{
    pin_write(dev, dev->cfg.cs_pin, TUYA_GPIO_LEVEL_HIGH);
    pin_write(dev, dev->cfg.cs_pin, TUYA_GPIO_LEVEL_LOW);
    pin_write(dev, dev->cfg.dc_pin, TUYA_GPIO_LEVEL_LOW);
    send_byte(dev, cmd);
    pin_write(dev, dev->cfg.dc_pin, TUYA_GPIO_LEVEL_HIGH);
    send_bytes(dev, data, len);
    /* Keep CS low: the vendor sample holds it through command/data/read. */
}

/** @brief Wait for active-low BUSY to release with a bounded timeout. */
static OPERATE_RET wait_ready(DISP_SSD2677_DEV_T *dev, const char *stage, uint32_t timeout_ms)
{
    OPERATE_RET rt = OPRT_OK;
    if (dev->io_result != OPRT_OK) {
        PR_ERR("SSD2677 I/O error at %s: %d", stage, dev->io_result);
        return dev->io_result;
    }
    TUYA_GPIO_LEVEL_E level;
    uint32_t          elapsed = 0;
    do {
        TUYA_CALL_ERR_RETURN(tkl_gpio_read(dev->cfg.busy_pin, &level));
        if (level == TUYA_GPIO_LEVEL_HIGH) {
            PR_DEBUG("SSD2677 %s ready after %u ms (BUSY=1)", stage, elapsed);
            return OPRT_OK;
        }
        tal_system_sleep(10);
        elapsed += 10;
    } while (elapsed < timeout_ms);
    PR_ERR("SSD2677 %s timeout %u ms, BUSY=0; check power/wiring/polarity", stage, elapsed);
    return OPRT_COM_ERROR;
}

/** @brief Configure a pin and track successful acquisition for cleanup. */
static OPERATE_RET init_pin(DISP_SSD2677_DEV_T *dev, TUYA_GPIO_NUM_E pin, bool input, TUYA_GPIO_LEVEL_E level)
{
    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode   = input ? TUYA_GPIO_PULLUP : TUYA_GPIO_PUSH_PULL,
        .direct = input ? TUYA_GPIO_INPUT : TUYA_GPIO_OUTPUT,
        .level  = level,
    };
    OPERATE_RET rt = tkl_gpio_init(pin, &cfg);
    if (rt == OPRT_OK) {
        const TUYA_GPIO_NUM_E pins[] = {dev->cfg.clk_pin, dev->cfg.sda_pin, dev->cfg.cs_pin,
                                        dev->cfg.dc_pin,  dev->cfg.rst_pin, dev->cfg.busy_pin};
        for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
            if (pins[i] == pin) {
                dev->initialized_pins |= (1u << i);
            }
        }
    }
    return rt;
}

/** @brief Release SPI before GPIO handover or closing the display. */
static OPERATE_RET stop_spi(DISP_SSD2677_DEV_T *dev)
{
    if (!dev->spi_initialized) {
        return OPRT_OK;
    }
    OPERATE_RET rt = tkl_spi_deinit(dev->cfg.port);
    if (rt == OPRT_OK) {
        dev->spi_initialized = false;
    }
    return rt;
}

/** @brief Restore hardware pin mux, then take CS back as a manual GPIO. */
static OPERATE_RET start_spi(DISP_SSD2677_DEV_T *dev)
{
    const TUYA_SPI_BASE_CFG_T cfg = {
        .role          = TUYA_SPI_ROLE_MASTER,
        .mode          = TUYA_SPI_MODE0,
        .type          = TUYA_SPI_SOFT_TYPE,
        .databits      = TUYA_SPI_DATA_BIT8,
        .bitorder      = TUYA_SPI_ORDER_MSB2LSB,
        .freq_hz       = dev->cfg.spi_clk,
        .spi_dma_flags = 0,
    };
    OPERATE_RET rt = tkl_spi_init(dev->cfg.port, &cfg);
    if (rt != OPRT_OK) {
        return rt;
    }
    dev->spi_initialized = true;
    /* Some adapters map CS even with SOFT_TYPE. Reclaim it after SPI init
     * so commands and their data share the vendor's manual CS sequence. */
    return init_pin(dev, dev->cfg.cs_pin, false, TUYA_GPIO_LEVEL_HIGH);
}

/** @brief Release only GPIOs acquired by this device. */
static void release_pins(DISP_SSD2677_DEV_T *dev)
{
    stop_spi(dev);
    const TUYA_GPIO_NUM_E pins[] = {dev->cfg.clk_pin, dev->cfg.sda_pin, dev->cfg.cs_pin,
                                    dev->cfg.dc_pin,  dev->cfg.rst_pin, dev->cfg.busy_pin};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if (dev->initialized_pins & (1u << i)) {
            tkl_gpio_deinit(pins[i]);
        }
    }
    dev->initialized_pins = 0;
}

/** @brief Reset and configure the validated E0397A09-A0 OTP profile. */
static OPERATE_RET panel_init(DISP_SSD2677_DEV_T *dev)
{
    OPERATE_RET rt   = OPRT_OK;
    dev->io_result   = OPRT_OK;
    dev->frame_ready = false;
    TUYA_CALL_ERR_RETURN(stop_spi(dev));
    TUYA_CALL_ERR_RETURN(init_pin(dev, dev->cfg.cs_pin, false, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(init_pin(dev, dev->cfg.clk_pin, false, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(init_pin(dev, dev->cfg.sda_pin, false, TUYA_GPIO_LEVEL_LOW));
    TUYA_CALL_ERR_RETURN(init_pin(dev, dev->cfg.dc_pin, false, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(init_pin(dev, dev->cfg.rst_pin, false, TUYA_GPIO_LEVEL_HIGH));
    TUYA_CALL_ERR_RETURN(init_pin(dev, dev->cfg.busy_pin, true, TUYA_GPIO_LEVEL_LOW));

    for (int i = 0; i < 2; ++i) {
        pin_write(dev, dev->cfg.rst_pin, TUYA_GPIO_LEVEL_LOW);
        tal_system_sleep(20);
        pin_write(dev, dev->cfg.rst_pin, TUYA_GPIO_LEVEL_HIGH);
        tal_system_sleep(10);
    }
    TUYA_CALL_ERR_RETURN(wait_ready(dev, "reset", 3000));

    TUYA_CALL_ERR_RETURN(start_spi(dev));
    /* UD=0 scans gates in reverse order. Together with the leading white
     * rows in panel_flush, this gives a top-left origin without flipping
     * the caller's framebuffer or traversing its rows backwards. */
    const uint8_t psr[] = {0x27, 0x0E};
    command(dev, SSD2677_CMD_PANEL_SETTING, psr, sizeof(psr));
    TUYA_CALL_ERR_RETURN(wait_ready(dev, "panel setting", 3000));
    const uint8_t booster[] = {0x0F, 0x8B, 0x93, 0xC1};
    command(dev, SSD2677_CMD_BOOSTER_SOFT_START, booster, sizeof(booster));
    command(dev, SSD2677_CMD_VENDOR_E7, (const uint8_t[]){0xC1}, 1);
    command(dev, SSD2677_CMD_FRAME_RATE_CONTROL, (const uint8_t[]){0x08}, 1);
    command(dev, SSD2677_CMD_VCOM_DATA_INTERVAL, (const uint8_t[]){0x77}, 1);
    const uint8_t timing[] = {0x76, 0x76, 0x76, 0x5A, 0x9D, 0x8A, 0x76, 0x62};
    command(dev, SSD2677_CMD_VENDOR_HTOTAL, timing, sizeof(timing));
    /* The 0x61 height field is independent of the visible framebuffer.
     * E0397A09-A0 A/B tests with this OTP profile: 680 refreshes all 480
     * visible rows; 480 leaves missing bands and old image content.
     * This does not imply a 680-row visible panel. */
    const uint8_t resolution[] = {dev->cfg.width >> 8, dev->cfg.width & 0xFF, dev->scan_height >> 8,
                                  dev->scan_height & 0xFF};
    command(dev, SSD2677_CMD_RESOLUTION_SETTING, resolution, sizeof(resolution));
    command(dev, SSD2677_CMD_CASCADE_TEMPERATURE_SETTING, (const uint8_t[]){0x10}, 1);
    command(dev, SSD2677_CMD_GATE_SOURCE_START, (const uint8_t[]){0, 0, 0, 0}, 4);
    command(dev, SSD2677_CMD_VENDOR_E9, (const uint8_t[]){0x01}, 1);
    tal_system_sleep(10);
    return wait_ready(dev, "OTP init", 3000);
}

/** @brief Read temperature on SDA and restore its output direction on error. */
static OPERATE_RET read_temperature(DISP_SSD2677_DEV_T *dev, uint8_t *value)
{
    OPERATE_RET rt = OPRT_OK;
    command(dev, SSD2677_CMD_TEMPERATURE_SENSOR, NULL, 0);
    tal_system_sleep(10);
    TUYA_CALL_ERR_RETURN(wait_ready(dev, "temperature", 3000));
    TUYA_CALL_ERR_RETURN(stop_spi(dev));
    rt = init_pin(dev, dev->cfg.clk_pin, false, TUYA_GPIO_LEVEL_LOW);
    if (rt == OPRT_OK) {
        rt = init_pin(dev, dev->cfg.sda_pin, true, TUYA_GPIO_LEVEL_LOW);
    }
    if (rt != OPRT_OK) {
        /* Attempt restoration even when GPIO handover only partially succeeded. */
        OPERATE_RET restore_rt = start_spi(dev);
        if (restore_rt != OPRT_OK) {
            PR_ERR("SSD2677 SPI restoration failed: %d", restore_rt);
        }
        return rt;
    }
    pin_write(dev, dev->cfg.cs_pin, TUYA_GPIO_LEVEL_HIGH);
    pin_write(dev, dev->cfg.cs_pin, TUYA_GPIO_LEVEL_LOW);
    uint8_t data = 0;
    for (int bit = 0; bit < 8; ++bit) {
        TUYA_GPIO_LEVEL_E level;
        rt = tkl_gpio_read(dev->cfg.sda_pin, &level);
        if (rt != OPRT_OK) {
            break;
        }
        data = (data << 1) | (level == TUYA_GPIO_LEVEL_HIGH);
        pin_write(dev, dev->cfg.clk_pin, TUYA_GPIO_LEVEL_HIGH);
        pin_write(dev, dev->cfg.clk_pin, TUYA_GPIO_LEVEL_LOW);
    }
    OPERATE_RET cs_rt = tkl_gpio_write(dev->cfg.cs_pin, TUYA_GPIO_LEVEL_HIGH);
    if (rt == OPRT_OK) {
        rt = dev->io_result != OPRT_OK ? dev->io_result : cs_rt;
    }
    OPERATE_RET restore_rt = start_spi(dev);
    if (rt != OPRT_OK || restore_rt != OPRT_OK) {
        return rt != OPRT_OK ? rt : restore_rt;
    }
    *value = data;
    PR_DEBUG("SSD2677 temperature raw=0x%02x (%d C)", data, (int)(int8_t)data);
    return dev->io_result;
}

/** @brief Restore internal temperature sensing and load the full-refresh OTP waveform. */
static OPERATE_RET load_full_waveform(DISP_SSD2677_DEV_T *dev)
{
    OPERATE_RET rt = OPRT_OK;
    /* E6 leaves a forced waveform temperature behind. The vendor resets
     * before each slow-refresh example; restore internal sensing likewise.
     */
    TUYA_CALL_ERR_RETURN(panel_init(dev));
    uint8_t temp = 0;
    TUYA_CALL_ERR_RETURN(read_temperature(dev, &temp));
    /* Temperature mapping from the vendor's slow-refresh OTP example. */
    uint8_t waveform_temp = temp == 0     ? 232
                            : temp <= 10  ? 235
                            : temp <= 20  ? 238
                            : temp <= 30  ? 241
                            : temp <= 127 ? 244
                                          : 232;
    command(dev, SSD2677_CMD_CASCADE_TEMPERATURE_SETTING, (const uint8_t[]){0x12}, 1);
    command(dev, SSD2677_CMD_TEMPERATURE_OVERRIDE, &waveform_temp, 1);
    command(dev, SSD2677_CMD_VENDOR_LOAD_WAVEFORM, NULL, 0);
    tal_system_sleep(10);
    return wait_ready(dev, "waveform load", 3000);
}

/** @brief Power on, require a real BUSY pulse, refresh and power off. */
static OPERATE_RET refresh_sequence(DISP_SSD2677_DEV_T *dev)
{
    OPERATE_RET rt = OPRT_OK;
    command(dev, SSD2677_CMD_POWER_ON, NULL, 0);
    tal_system_sleep(10);
    TUYA_CALL_ERR_RETURN(wait_ready(dev, "power on", 5000));
    command(dev, SSD2677_CMD_DISPLAY_REFRESH, (const uint8_t[]){0x00}, 1);
    if (dev->io_result != OPRT_OK) {
        return dev->io_result;
    }
    /* A disconnected BUSY input is pulled high. Require an actual busy pulse
     * so a floating input cannot be mistaken for a successful screen update.
     */
    bool busy_seen = false;
    for (unsigned ms = 0; ms < 200; ++ms) {
        TUYA_GPIO_LEVEL_E level;
        TUYA_CALL_ERR_RETURN(tkl_gpio_read(dev->cfg.busy_pin, &level));
        if (level == TUYA_GPIO_LEVEL_LOW) {
            busy_seen = true;
            break;
        }
        tal_system_sleep(1);
    }
    if (!busy_seen) {
        PR_ERR("SSD2677 refresh did not assert BUSY; check SPI, reset and BUSY wiring");
        return OPRT_COM_ERROR;
    }
    PR_DEBUG("SSD2677 refresh BUSY asserted");
    TUYA_CALL_ERR_RETURN(wait_ready(dev, "refresh", 60000));
    command(dev, SSD2677_CMD_POWER_OFF, (const uint8_t[]){0x00}, 1);
    tal_system_sleep(20);
    return wait_ready(dev, "power off", 5000);
}

/** @brief On refresh failure, attempt power-off only when BUSY confirms idle. */
static OPERATE_RET refresh(DISP_SSD2677_DEV_T *dev)
{
    OPERATE_RET rt = refresh_sequence(dev);
    if (rt != OPRT_OK) {
        TUYA_GPIO_LEVEL_E level;
        if (tkl_gpio_read(dev->cfg.busy_pin, &level) == OPRT_OK && level == TUYA_GPIO_LEVEL_HIGH) {
            dev->io_result = OPRT_OK;
            command(dev, SSD2677_CMD_POWER_OFF, (const uint8_t[]){0x00}, 1);
            tal_system_sleep(20);
            OPERATE_RET cleanup_rt = wait_ready(dev, "error power off", 5000);
            if (cleanup_rt != OPRT_OK) {
                PR_ERR("SSD2677 error power off failed: %d", cleanup_rt);
            }
        }
        /* Preserve the original failure; only a full refresh restores the baseline. */
    }
    return rt;
}

/** @brief Initialize one registered display and unwind partial GPIO acquisition. */
static OPERATE_RET panel_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SSD2677_DEV_T *dev = (DISP_SSD2677_DEV_T *)device;
    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }
    if (dev->opened) {
        return OPRT_OK;
    }
    OPERATE_RET rt = panel_init(dev);
    if (rt != OPRT_OK) {
        release_pins(dev);
    }
    dev->opened = rt == OPRT_OK;
    return rt;
}

/** @brief Expand packed TDL rows into controller 2-bit pixels without copying a frame. */
static OPERATE_RET send_frame(DISP_SSD2677_DEV_T *dev, const TDL_DISP_FRAME_BUFF_T *fb)
{
    uint8_t wire[256];
    size_t  row_bytes = fb->width / 8;
    for (size_t y = 0; y < fb->height; ++y) {
        /* TDL pixels are LSB-first,
         * 1 black / 0 white; wire pixels are MSB-first 00 black / 01 white. */
        for (size_t offset = 0; offset < row_bytes;) {
            size_t count = row_bytes - offset;
            if (count > sizeof(wire) / 2) {
                count = sizeof(wire) / 2;
            }
            for (size_t x = 0; x < count; ++x) {
                uint8_t  value  = fb->frame[y * row_bytes + offset + x];
                uint16_t packed = 0;
                for (unsigned bit = 0; bit < 8; ++bit) {
                    packed = (packed << 2) | (((value >> bit) & 1) ^ 1);
                }
                wire[x * 2]     = packed >> 8;
                wire[x * 2 + 1] = packed & 0xFF;
            }
            send_bytes(dev, wire, count * 2);
            if (dev->io_result != OPRT_OK) {
                return dev->io_result;
            }
            offset += count;
        }
    }
    return OPRT_OK;
}

/** @brief Set an inclusive controller window with the validated UD=0 row offset. */
static void partial_window(DISP_SSD2677_DEV_T *dev, const TDL_DISP_FRAME_BUFF_T *fb)
{
    uint16_t      x0 = fb->x_start, x1 = x0 + fb->width - 1;
    uint16_t      y0       = dev->scan_height - dev->cfg.height + fb->y_start;
    uint16_t      y1       = y0 + fb->height - 1;
    const uint8_t window[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF, y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF, 1};
    command(dev, SSD2677_CMD_PARTIAL_WINDOW, window, sizeof(window));
}

/** @brief Refresh a full frame, or a byte-aligned window after a successful full frame. */
static OPERATE_RET panel_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *fb)
{
    OPERATE_RET         rt  = OPRT_OK;
    DISP_SSD2677_DEV_T *dev = (DISP_SSD2677_DEV_T *)device;
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    if (!dev->opened) {
        return OPRT_COM_ERROR;
    }
    if (!fb || !fb->frame || fb->fmt != TUYA_PIXEL_FMT_MONOCHROME || !fb->width || !fb->height || fb->x_start % 8 ||
        fb->width % 8 || (uint32_t)fb->x_start + fb->width > dev->cfg.width ||
        (uint32_t)fb->y_start + fb->height > dev->cfg.height || fb->len != (uint32_t)(fb->width / 8) * fb->height) {
        return OPRT_INVALID_PARM;
    }
    bool full = !fb->x_start && !fb->y_start && fb->width == dev->cfg.width && fb->height == dev->cfg.height;
    if (!full && !dev->frame_ready) {
        PR_ERR("SSD2677 partial refresh requires a successful full frame first");
        return OPRT_COM_ERROR;
    }
    dev->frame_ready = false;
    if (full) {
        TUYA_CALL_ERR_RETURN(load_full_waveform(dev));
    } else {
        /* Preserve SRAM and the loaded waveform; reset would discard the baseline. */
        partial_window(dev, fb);
    }
    command(dev, SSD2677_CMD_DATA_START_TRANSMISSION, NULL, 0);
    uint16_t pad_rows = full ? dev->scan_height - dev->cfg.height : 0;
    uint8_t  white[256];
    memset(white, 0x55, sizeof(white));
    for (uint16_t y = 0; y < pad_rows; ++y) {
        for (size_t remaining = dev->cfg.width / 4; remaining;) {
            size_t count = remaining > sizeof(white) ? sizeof(white) : remaining;
            send_bytes(dev, white, count);
            if (dev->io_result != OPRT_OK) {
                return dev->io_result;
            }
            remaining -= count;
        }
    }
    TUYA_CALL_ERR_RETURN(send_frame(dev, fb));
    PR_DEBUG("SSD2677 %s: x=%u y=%u %ux%u, %u mono bytes, %u padding rows, %u wire bytes", full ? "full" : "partial",
             fb->x_start, fb->y_start, fb->width, fb->height, (unsigned)fb->len, pad_rows,
             (unsigned)fb->width * (fb->height + pad_rows) / 4);
    TUYA_CALL_ERR_RETURN(wait_ready(dev, "image data", 3000));
    TUYA_CALL_ERR_RETURN(refresh(dev));
    dev->frame_ready = true;
    return OPRT_OK;
}

/** @brief Enter deep sleep only when idle, then release bus pins. */
static OPERATE_RET panel_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SSD2677_DEV_T *dev = (DISP_SSD2677_DEV_T *)device;
    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }
    if (!dev->opened) {
        return OPRT_OK;
    }
    /* A past transfer error must not prevent a fresh close attempt. */
    dev->io_result = OPRT_OK;
    OPERATE_RET rt = wait_ready(dev, "sleep", 3000);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = dev->spi_initialized ? OPRT_OK : start_spi(dev);
    if (rt != OPRT_OK) {
        return rt;
    }
    dev->frame_ready = false;
    command(dev, SSD2677_CMD_DEEP_SLEEP, (const uint8_t[]){0xA5}, 1);
    pin_write(dev, dev->cfg.cs_pin, TUYA_GPIO_LEVEL_HIGH);
    if (dev->io_result != OPRT_OK) {
        return dev->io_result;
    }
    tal_system_sleep(20);
    rt = stop_spi(dev);
    if (rt != OPRT_OK) {
        return rt;
    }
    release_pins(dev);
    dev->opened      = false;
    dev->frame_ready = false;
    PR_DEBUG("SSD2677 deep sleep");
    return rt;
}

/** @brief Serialize lifecycle operations and release frames outside the device lock. */
static OPERATE_RET locked_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SSD2677_DEV_T *dev = device;
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET rt = tal_mutex_lock(dev->mutex);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = panel_open(device);
    tal_mutex_unlock(dev->mutex);
    return rt;
}

static OPERATE_RET locked_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *fb)
{
    DISP_SSD2677_DEV_T *dev = device;
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET rt = tal_mutex_lock(dev->mutex);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = panel_flush(device, fb);
    tal_mutex_unlock(dev->mutex);
    if (rt == OPRT_OK && fb->free_cb) {
        fb->free_cb(fb);
    }
    return rt;
}

static OPERATE_RET locked_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_SSD2677_DEV_T *dev = device;
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    OPERATE_RET rt = tal_mutex_lock(dev->mutex);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = panel_close(device);
    tal_mutex_unlock(dev->mutex);
    return rt;
}

/**
 * @brief Register the validated E0397A09-A0 hardware-SPI panel profile.
 * @param name Unique display device name.
 * @param dev_cfg Required signal pins and optional power control.
 * @return SDK operation status; allocated state is freed on registration failure.
 */
OPERATE_RET tdd_disp_spi_mono_ssd2677_register(char *name, const DISP_EINK_SSD2677_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg || name[0] == '\0' || strlen(name) > DISPLAY_DEV_NAME_MAX_LEN ||
        NULL != tdl_disp_find_dev(name)) {
        return OPRT_INVALID_PARM;
    }
    uint16_t scan_height = dev_cfg->controller_height ? dev_cfg->controller_height : dev_cfg->height;
    if ((unsigned)dev_cfg->port >= TUYA_SPI_NUM_MAX || !dev_cfg->spi_clk || !dev_cfg->width || (dev_cfg->width % 8) ||
        dev_cfg->width > SSD2677_SOURCE_MAX || !dev_cfg->height || dev_cfg->height > scan_height ||
        scan_height < SSD2677_GATE_MIN || scan_height > SSD2677_GATE_MAX) {
        return OPRT_INVALID_PARM;
    }
    const TUYA_GPIO_NUM_E pins[] = {dev_cfg->clk_pin, dev_cfg->sda_pin, dev_cfg->cs_pin,
                                    dev_cfg->dc_pin,  dev_cfg->rst_pin, dev_cfg->busy_pin};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        if ((unsigned)pins[i] >= TUYA_GPIO_NUM_MAX) {
            return OPRT_INVALID_PARM;
        }
        for (size_t j = 0; j < i; ++j) {
            if (pins[i] == pins[j]) {
                return OPRT_INVALID_PARM;
            }
        }
        if (pins[i] == dev_cfg->power.pin) {
            return OPRT_INVALID_PARM;
        }
    }
    if ((unsigned)dev_cfg->power.pin > TUYA_GPIO_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }
    DISP_SSD2677_DEV_T *dev = tal_malloc(sizeof(*dev));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(*dev));
    dev->cfg         = *dev_cfg;
    dev->scan_height = scan_height;
    OPERATE_RET rt   = tal_mutex_create_init(&dev->mutex);
    if (rt != OPRT_OK) {
        tal_free(dev);
        return rt;
    }
    TDD_DISP_INTFS_T    intfs = {.open = locked_open, .flush = locked_flush, .close = locked_close};
    TDD_DISP_DEV_INFO_T info  = {
         .type     = TUYA_DISPLAY_SPI,
         .width    = dev_cfg->width,
         .height   = dev_cfg->height,
         .has_vram = true,
         .fmt      = TUYA_PIXEL_FMT_MONOCHROME,
         .rotation = TUYA_DISPLAY_ROTATION_0,
         .bl       = {.type = TUYA_DISP_BL_TP_NONE},
         .power    = dev_cfg->power,
    };
    rt = tdl_disp_device_register(name, dev, &intfs, &info);
    if (rt != OPRT_OK) {
        tal_mutex_release(dev->mutex);
        tal_free(dev);
    }
    return rt;
}

/**
 * @file tdd_disp_esp_lcd.c
 * @brief Adapter that wraps an ESP-IDF esp_lcd_panel into a TuyaOpen TDD display driver.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_lcd.h"

#include "tal_memory.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "tdd_disp_esp_lcd"

/* SPI DMA on ESP32-S3 requires internal SRAM. PSRAM is not DMA-capable
 * (esp_ptr_dma_capable returns false for PSRAM addresses). Copy the VRAM
 * frame buffer through internal-SRAM bounce buffers before transmit.
 *
 * Two configurations are supported:
 *  - Single-shot: a single buffer big enough for the whole frame. Only one
 *    draw_bitmap is issued per flush, so there is no chunk race.
 *  - Tiled ping-pong: two equal-sized buffers; CPU writes buf[N^1] while DMA
 *    reads buf[N]. ESP-IDF's tx_param waits for the previous color tx before
 *    issuing the next CASET, so alternating buffers eliminates the race.
 *
 * Memcpy MUST go to the buffer that is NOT currently being read by DMA, and
 * draw_bitmap MUST be called BEFORE the next memcpy on the same buffer. */
#define BOUNCE_BUF_FALLBACK_LINES 8

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    void                   *panel_io;
    void                   *panel;
    TDD_DISP_ESP_LCD_CFG_T  cfg;
    void                   *bounce_buf[2];    /* [0] always valid; [1] valid only in tiled (ping-pong) mode */
    uint32_t                bounce_buf_lines; /* number of full RGB565 rows that fit in one bounce buffer */
    bool                    single_shot;      /* true if bounce_buf[0] alone covers the whole frame */
    int                     bounce_buf_idx;   /* next buffer index to write (tiled mode only) */
} DISP_ESP_LCD_DEV_T;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __esp_lcd_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_LCD_DEV_T *dev = (DISP_ESP_LCD_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel) {
        return OPRT_INVALID_PARM;
    }

    esp_lcd_panel_disp_on_off((esp_lcd_panel_handle_t)dev->panel, true);

    return OPRT_OK;
}

static OPERATE_RET __esp_lcd_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_ESP_LCD_DEV_T *dev = (DISP_ESP_LCD_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    int x1 = frame_buff->x_start;
    int y1 = frame_buff->y_start;
    int x2 = frame_buff->x_start + frame_buff->width;
    int y2 = frame_buff->y_start + frame_buff->height;

    if (dev->bounce_buf[0] && dev->bounce_buf_lines > 0) {
        /* Frame buffer is in PSRAM which is not DMA-capable on ESP32-S3.
         * Copy through internal-SRAM bounce buffer(s) before queueing DMA. */
        uint8_t *src = (uint8_t *)frame_buff->frame;
        int bytes_per_line = frame_buff->width * 2; /* RGB565 */
        int bounce_lines = (int)dev->bounce_buf_lines;
        int y = y1;

        if (dev->single_shot) {
            /* Whole frame fits in one buffer: single memcpy + single draw_bitmap.
             * No race possible: only one DMA in flight per flush. */
            memcpy(dev->bounce_buf[0], src, (size_t)bytes_per_line * (y2 - y1));
            esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)dev->panel,
                                      x1, y1, x2, y2,
                                      dev->bounce_buf[0]);
        } else {
            /* Tiled ping-pong: alternate between two equally-sized buffers so the
             * CPU writes buf[idx] while DMA reads buf[idx ^ 1]. ESP-IDF's
             * panel_io_spi_tx_param waits for any in-flight color tx before
             * issuing the next CASET, ensuring we never overwrite a buffer that
             * is still being DMA'd out. */
            int buf_idx = dev->bounce_buf_idx;
            while (y < y2) {
                int lines = (y2 - y < bounce_lines) ? (y2 - y) : bounce_lines;
                memcpy(dev->bounce_buf[buf_idx], src, lines * bytes_per_line);
                esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)dev->panel,
                                          x1, y, x2, y + lines,
                                          dev->bounce_buf[buf_idx]);
                src += lines * bytes_per_line;
                y   += lines;
                buf_idx ^= 1;
            }
            dev->bounce_buf_idx = buf_idx;
        }
    } else {
        esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)dev->panel, x1, y1, x2, y2, frame_buff->frame);
    }

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __esp_lcd_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_LCD_DEV_T *dev = (DISP_ESP_LCD_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel) {
        return OPRT_INVALID_PARM;
    }

    esp_lcd_panel_disp_on_off((esp_lcd_panel_handle_t)dev->panel, false);

    return OPRT_OK;
}

OPERATE_RET tdd_disp_esp_lcd_register(char *name, void *panel_io, void *panel,
                                       TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == name || NULL == panel || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    DISP_ESP_LCD_DEV_T *dev = tal_malloc(sizeof(DISP_ESP_LCD_DEV_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(DISP_ESP_LCD_DEV_T));

    dev->panel_io = panel_io;
    dev->panel    = panel;
    memcpy(&dev->cfg, cfg, sizeof(TDD_DISP_ESP_LCD_CFG_T));

    /* First try a single full-frame DMA-capable bounce buffer (single-shot,
     * no chunking, no race). If that fails, fall back to two equal-sized
     * ping-pong buffers, halving the size each iteration until they fit. */
    int bytes_per_line = cfg->width * 2; /* RGB565 */
    size_t full_size = (size_t)bytes_per_line * cfg->height;
    dev->bounce_buf[0] = heap_caps_malloc(full_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (dev->bounce_buf[0]) {
        dev->bounce_buf[1]    = NULL;
        dev->bounce_buf_lines = cfg->height;
        dev->single_shot      = true;
        ESP_LOGI(TAG, "bounce buf alloc %u B (%d lines, single-shot)",
                 (unsigned)full_size, cfg->height);
    } else {
        int try_lines = cfg->height / 2;
        while (try_lines >= BOUNCE_BUF_FALLBACK_LINES) {
            size_t bounce_size = (size_t)bytes_per_line * try_lines;
            dev->bounce_buf[0] = heap_caps_malloc(bounce_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            dev->bounce_buf[1] = heap_caps_malloc(bounce_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            if (dev->bounce_buf[0] && dev->bounce_buf[1]) {
                dev->bounce_buf_lines = (uint32_t)try_lines;
                dev->single_shot      = false;
                dev->bounce_buf_idx   = 0;
                ESP_LOGI(TAG, "bounce buf alloc 2x%u B (%d lines, tiled ping-pong)",
                         (unsigned)bounce_size, try_lines);
                break;
            }
            if (dev->bounce_buf[0]) { heap_caps_free(dev->bounce_buf[0]); dev->bounce_buf[0] = NULL; }
            if (dev->bounce_buf[1]) { heap_caps_free(dev->bounce_buf[1]); dev->bounce_buf[1] = NULL; }
            try_lines /= 2;
        }
    }
    if (NULL == dev->bounce_buf[0]) {
        dev->bounce_buf_lines = 0;
        ESP_LOGW(TAG, "bounce buf alloc failed; direct PSRAM DMA will be attempted (likely to fail)");
    }

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_SPI,
        .width    = cfg->width,
        .height   = cfg->height,
        .fmt      = cfg->pixel_fmt,
        .rotation = cfg->rotation,
        .is_swap  = cfg->is_swap,
        .has_vram = true,
    };
    memcpy(&dev_info.bl,    &cfg->bl,    sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&dev_info.power, &cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T intfs = {
        .open  = __esp_lcd_open,
        .flush = __esp_lcd_flush,
        .close = __esp_lcd_close,
    };

    rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
    }

    return rt;
}

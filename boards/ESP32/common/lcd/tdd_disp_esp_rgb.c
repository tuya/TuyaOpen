/**
 * @file tdd_disp_esp_rgb.c
 * @brief ESP32-S3/S31 RGB parallel panel TDD display adapter.
 *
 * Uses esp_lcd_new_rgb_panel() to create a continuously-refreshed panel backed
 * by PSRAM frame buffers. Two modes by bounce_buffer_size:
 *  - 0  (direct scan): double internal fbs, exposed via get_frame_buffer for
 *    zero-copy flush (the pool draws straight into them, flush only switches
 *    the scan and waits for the previous fb to complete).
 *  - >0 (bounce): single internal fb, flush of an external buffer copies inside
 *    esp_lcd; fbs not exposed.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_rgb.h"

#include "tal_memory.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "tdd_disp_esp_rgb"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    esp_lcd_panel_handle_t      panel;
    TDD_DISP_ESP_LCD_CFG_T      cfg;
    TDD_DISP_ESP_RGB_HW_CFG_T   hw;
    SemaphoreHandle_t           trans_done_sem;
    /* direct-scan (zero-copy) mode only: the two internal fbs exposed to the fb pool */
    void                       *vram_fb[2];
    SemaphoreHandle_t           fb_done_sem;  /* given when the DMA link switches = previous fb done */
    TDL_DISP_FRAME_BUFF_T      *scan_fb;      /* pool fb committed for scanning; free_cb deferred */
    uint32_t                    frame_time_ms;/* one frame period, for the flush wait timeout */
} DISP_ESP_RGB_DEV_T;

/***********************************************************
***********************function define**********************
***********************************************************/

static bool IRAM_ATTR __rgb_on_vsync(esp_lcd_panel_handle_t panel,
                                     const esp_lcd_rgb_panel_event_data_t *edata,
                                     void *user_ctx)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)user_ctx;
    BaseType_t hp_task_woken = pdFALSE;

    if (dev && dev->trans_done_sem) {
        xSemaphoreGiveFromISR(dev->trans_done_sem, &hp_task_woken);
    }

    return hp_task_woken == pdTRUE;
}

/* stream mode + no bounce: fires only on the real DMA descriptor-chain switch,
 * i.e. exactly when the previously scanned fb stops being read. */
static bool IRAM_ATTR __rgb_on_fb_done(esp_lcd_panel_handle_t panel,
                                       const esp_lcd_rgb_panel_event_data_t *edata,
                                       void *user_ctx)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)user_ctx;
    BaseType_t hp_task_woken = pdFALSE;

    if (dev && dev->fb_done_sem) {
        xSemaphoreGiveFromISR(dev->fb_done_sem, &hp_task_woken);
    }

    return hp_task_woken == pdTRUE;
}

static OPERATE_RET __esp_rgb_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)device;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    /* Already opened? */
    if (dev->panel) {
        return OPRT_OK;
    }

    TDD_DISP_ESP_RGB_HW_CFG_T *hw = &dev->hw;

    /* bounce_buffer_size: 0 = direct scan from PSRAM (zero-copy, double fb);
     * >0 = bounce size in bytes (single fb, not exposed) */
    uint32_t bounce_buf_size = hw->bounce_buffer_size;
    bool direct_scan = (0 == bounce_buf_size);

    esp_lcd_rgb_panel_config_t panel_cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        /* IDF 6.x: trans_align/bits_per_pixel replaced by color formats */
        .data_width        = 16,
        .in_color_format   = LCD_COLOR_FMT_RGB565,
        .out_color_format  = LCD_COLOR_FMT_RGB565,
        .num_fbs           = direct_scan ? 2 : 1,
        .dma_burst_size    = 64,
        .bounce_buffer_size_px = bounce_buf_size / 2,
        .timings = {
            .pclk_hz            = hw->pclk_hz,
            .h_res              = hw->h_res,
            .v_res              = hw->v_res,
            .hsync_back_porch   = hw->hsync_back_porch,
            .hsync_front_porch  = hw->hsync_front_porch,
            .hsync_pulse_width  = hw->hsync_pulse_width,
            .vsync_back_porch   = hw->vsync_back_porch,
            .vsync_front_porch  = hw->vsync_front_porch,
            .vsync_pulse_width  = hw->vsync_pulse_width,
            .flags = {
                .pclk_active_neg = hw->pclk_active_neg,
                .de_idle_high    = 0,
                .pclk_idle_high  = 0,
                .hsync_idle_low  = 0,
                .vsync_idle_low  = 0,
            },
        },
        .flags = {
            .fb_in_psram         = 1,
            .no_fb               = 0,
            .bb_invalidate_cache = 0,
        },
    };

    panel_cfg.pclk_gpio_num  = hw->pclk_gpio;
    panel_cfg.de_gpio_num    = hw->de_gpio;
    panel_cfg.hsync_gpio_num = hw->hsync_gpio;
    panel_cfg.vsync_gpio_num = hw->vsync_gpio;
    panel_cfg.disp_gpio_num  = (hw->disp_gpio > 0) ? hw->disp_gpio : -1;
    for (int i = 0; i < 16; i++) {
        panel_cfg.data_gpio_nums[i] = hw->data_gpio[i];
    }

    esp_err_t err = esp_lcd_new_rgb_panel(&panel_cfg, &dev->panel);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "esp_lcd_new_rgb_panel failed: 0x%x", err);
        return OPRT_COM_ERROR;
    }

    /* Register VSYNC event callback */
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = __rgb_on_vsync,
        .on_frame_buf_complete = __rgb_on_fb_done,
    };
    esp_lcd_rgb_panel_register_event_callbacks(dev->panel, &cbs, dev);

    esp_lcd_panel_reset(dev->panel);
    esp_lcd_panel_init(dev->panel);

    if (direct_scan) {
        /* expose the two internal fbs: pool shells wrap them for zero-copy flush */
        esp_err_t fb_err = esp_lcd_rgb_panel_get_frame_buffer(dev->panel, 2,
                                                              &dev->vram_fb[0], &dev->vram_fb[1]);
        if (ESP_OK != fb_err) {
            ESP_LOGE(TAG, "get_frame_buffer failed: 0x%x", fb_err);
            esp_lcd_panel_del(dev->panel);
            dev->panel = NULL;
            return OPRT_COM_ERROR;
        }
        uint32_t h_total = hw->h_res + hw->hsync_pulse_width + hw->hsync_back_porch + hw->hsync_front_porch;
        uint32_t v_total = hw->v_res + hw->vsync_pulse_width + hw->vsync_back_porch + hw->vsync_front_porch;
        dev->frame_time_ms = (uint32_t)(h_total * v_total * 1000ULL / hw->pclk_hz) + 1;
        /* the panel starts scanning fb[0]; park it on fb[1] so the first pool fb
         * (fb[0]) is idle from the start and the first commit is a real switch */
        esp_lcd_panel_draw_bitmap(dev->panel, 0, 0, hw->h_res, hw->v_res, dev->vram_fb[1]);
        ESP_LOGI(TAG, "direct-scan mode: fb0=%p fb1=%p frame=%ums",
                 dev->vram_fb[0], dev->vram_fb[1], dev->frame_time_ms);
    }

    ESP_LOGI(TAG, "RGB panel opened: %dx%d @ %d Hz", hw->h_res, hw->v_res, hw->pclk_hz);
    return OPRT_OK;
}

static OPERATE_RET __esp_rgb_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    int x1 = frame_buff->x_start;
    int y1 = frame_buff->y_start;
    int x2 = frame_buff->x_start + frame_buff->width;
    int y2 = frame_buff->y_start + frame_buff->height;

    /* vram fb: esp_lcd recognizes the registered address and switches the scan
     * without any copy (zero-copy). */
    if (frame_buff->frame == dev->vram_fb[0] || frame_buff->frame == dev->vram_fb[1]) {
        esp_err_t err = esp_lcd_panel_draw_bitmap(dev->panel, 0, 0,
                                                  dev->hw.h_res, dev->hw.v_res,
                                                  frame_buff->frame);
        if (ESP_OK != err) {
            ESP_LOGE(TAG, "draw_bitmap failed: 0x%x", err);
            if (frame_buff->free_cb) {
                frame_buff->free_cb(frame_buff);
            }
            return OPRT_COM_ERROR;
        }

        /* wait for the link switch (= previous fb done scanning), then return
         * the previous fb to the pool. This fb's free_cb fires on the NEXT commit. */
        TDL_DISP_FRAME_BUFF_T *prev = dev->scan_fb;
        dev->scan_fb = frame_buff;

        if (pdTRUE != xSemaphoreTake(dev->fb_done_sem,
                                     pdMS_TO_TICKS(dev->frame_time_ms * 3 + 10))) {
            /* keep flowing, but the previous fb may still be scanning */
            ESP_LOGW(TAG, "frame-buf-complete timeout");
        }

        if (prev && prev != frame_buff && prev->free_cb) {
            prev->free_cb(prev);
        }

        return OPRT_OK;
    }

    /* heap fb: esp_lcd copies internally, caller's fb reusable right away */
    esp_err_t err = esp_lcd_panel_draw_bitmap(dev->panel, x1, y1, x2, y2, frame_buff->frame);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "draw_bitmap failed: 0x%x", err);
        if (frame_buff->free_cb) {
            frame_buff->free_cb(frame_buff);
        }
        return OPRT_COM_ERROR;
    }

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

/* Expose the driver-internal fbs (direct-scan mode only) for zero-copy flush. */
static OPERATE_RET __esp_rgb_get_frame_buffer(TDD_DISP_DEV_HANDLE_T device,
                                              uint8_t max_num, void **fbs, uint8_t *fb_num)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)device;

    if (NULL == dev || NULL == fbs || NULL == fb_num) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == dev->vram_fb[0]) {
        /* bounce mode or not opened */
        return OPRT_NOT_SUPPORTED;
    }

    uint8_t cnt = (max_num < 2) ? max_num : 2;
    for (uint8_t i = 0; i < cnt; i++) {
        fbs[i] = dev->vram_fb[i];
    }
    *fb_num = cnt;

    return OPRT_OK;
}

static OPERATE_RET __esp_rgb_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)device;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    if (dev->panel) {
        /* return the fb still committed for scanning, if any */
        if (dev->scan_fb && dev->scan_fb->free_cb) {
            dev->scan_fb->free_cb(dev->scan_fb);
        }
        dev->scan_fb = NULL;

        esp_lcd_panel_del(dev->panel);
        dev->panel = NULL;
        dev->vram_fb[0] = NULL;
        dev->vram_fb[1] = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET tdd_disp_esp_rgb_register(char *name, TDD_DISP_ESP_RGB_HW_CFG_T *hw,
                                      TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    if (NULL == name || NULL == hw || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    DISP_ESP_RGB_DEV_T *dev = tal_malloc(sizeof(DISP_ESP_RGB_DEV_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(DISP_ESP_RGB_DEV_T));
    memcpy(&dev->cfg, cfg, sizeof(TDD_DISP_ESP_LCD_CFG_T));
    memcpy(&dev->hw, hw, sizeof(TDD_DISP_ESP_RGB_HW_CFG_T));

    dev->trans_done_sem = xSemaphoreCreateBinary();
    if (NULL == dev->trans_done_sem) {
        tal_free(dev);
        return OPRT_MALLOC_FAILED;
    }

    dev->fb_done_sem = xSemaphoreCreateBinary();
    if (NULL == dev->fb_done_sem) {
        vSemaphoreDelete(dev->trans_done_sem);
        tal_free(dev);
        return OPRT_MALLOC_FAILED;
    }

    /* Register with TuyaOpen display framework (no HW init here) */
    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_RGB,
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
        .open  = __esp_rgb_open,
        .flush = __esp_rgb_flush,
        .close = __esp_rgb_close,
        .get_frame_buffer = __esp_rgb_get_frame_buffer,
    };

    OPERATE_RET rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        vSemaphoreDelete(dev->fb_done_sem);
        vSemaphoreDelete(dev->trans_done_sem);
        tal_free(dev);
    }

    return rt;
}

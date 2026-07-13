/**
 * @file tdd_disp_esp_rgb.c
 * @brief ESP32-S3 RGB parallel panel TDD display adapter.
 *
 * Uses esp_lcd_new_rgb_panel() to create a continuously-refreshed panel backed
 * by PSRAM frame buffers. Flush copies a region from VRAM into the panel's
 * internal frame buffer via esp_lcd_panel_draw_bitmap().
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

/* Default bounce buffer: 40 lines of RGB565.
 * Larger buffer = fewer DMA interrupts = less PSRAM bandwidth contention = no stripes.
 * 800 * 40 * 2 = 64KB per bounce buffer (ESP-IDF allocates 2 internally). */
#define DEFAULT_BOUNCE_BUF_LINES 40

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    esp_lcd_panel_handle_t      panel;
    TDD_DISP_ESP_LCD_CFG_T      cfg;
    TDD_DISP_ESP_RGB_HW_CFG_T   hw;
    SemaphoreHandle_t           trans_done_sem;
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

    /* Calculate bounce buffer size */
    uint32_t bounce_buf_size = hw->bounce_buffer_size;
    if (0 == bounce_buf_size) {
        bounce_buf_size = hw->h_res * DEFAULT_BOUNCE_BUF_LINES * 2;
    }

    esp_lcd_rgb_panel_config_t panel_cfg = {
        .clk_src           = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .sram_trans_align  = 4,
        .data_width        = 16,
        .bits_per_pixel    = 16,
        .num_fbs           = 1,
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
                .pclk_active_neg = 0,
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
    panel_cfg.disp_gpio_num  = -1;
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
    };
    esp_lcd_rgb_panel_register_event_callbacks(dev->panel, &cbs, dev);

    esp_lcd_panel_reset(dev->panel);
    esp_lcd_panel_init(dev->panel);

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

    esp_err_t err = esp_lcd_panel_draw_bitmap(dev->panel, x1, y1, x2, y2, frame_buff->frame);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "draw_bitmap failed: 0x%x", err);
        if (frame_buff->free_cb) {
            frame_buff->free_cb(frame_buff);
        }
        return OPRT_COM_ERROR;
    }

    /* RGB panel continuously refreshes from PSRAM framebuffer.
     * draw_bitmap copies data into the framebuffer and returns immediately.
     * No need to wait for VSYNC - skip it for higher FPS (may cause tearing). */

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __esp_rgb_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_RGB_DEV_T *dev = (DISP_ESP_RGB_DEV_T *)device;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    if (dev->panel) {
        esp_lcd_panel_del(dev->panel);
        dev->panel = NULL;
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
    };

    OPERATE_RET rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        vSemaphoreDelete(dev->trans_done_sem);
        tal_free(dev);
    }

    return rt;
}

/**
 * @file tdd_disp_esp_dpi.c
 * @brief Adapter that wraps an ESP-IDF MIPI-DSI DPI panel into a TuyaOpen TDD
 *        display driver.
 *
 * A DPI panel owns its frame buffer(s) in PSRAM and is refreshed continuously by
 * hardware, so a flush is a single esp_lcd_panel_draw_bitmap() that copies the
 * VRAM frame into the internal frame buffer. The panel decides CPU vs DMA2D copy
 * via its use_dma2d flag; with DMA2D the copy is asynchronous, so we wait for the
 * panel's on_color_trans_done event before recycling the VRAM frame.
 *
 * No internal-SRAM bounce buffering and no per-line chunking (those belong to the
 * framebuffer-less SPI adapter, tdd_disp_esp_spi.c).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_disp_esp_dpi.h"

#include "tal_memory.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "tdd_disp_esp_dpi"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    void                  *panel_io;
    void                  *panel;
    TDD_DISP_ESP_LCD_CFG_T cfg;
    SemaphoreHandle_t      trans_done_sem; /* given when the draw buffer copy completes */
} DISP_ESP_DPI_DEV_T;

/***********************************************************
***********************function define**********************
***********************************************************/

/* Invoked from the DMA2D done ISR once the panel has finished consuming the
 * source frame buffer. Unblocks the waiting flush. */
static bool __dpi_color_trans_done(esp_lcd_panel_handle_t panel,
                                   esp_lcd_dpi_panel_event_data_t *edata,
                                   void *user_ctx)
{
    DISP_ESP_DPI_DEV_T *dev = (DISP_ESP_DPI_DEV_T *)user_ctx;
    BaseType_t hp_task_woken = pdFALSE;

    if (dev && dev->trans_done_sem) {
        xSemaphoreGiveFromISR(dev->trans_done_sem, &hp_task_woken);
    }

    return hp_task_woken == pdTRUE;
}

static OPERATE_RET __esp_dpi_open(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_DPI_DEV_T *dev = (DISP_ESP_DPI_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel) {
        return OPRT_INVALID_PARM;
    }

    esp_lcd_panel_disp_on_off((esp_lcd_panel_handle_t)dev->panel, true);

    return OPRT_OK;
}

static OPERATE_RET __esp_dpi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    DISP_ESP_DPI_DEV_T *dev = (DISP_ESP_DPI_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    int x1 = frame_buff->x_start;
    int y1 = frame_buff->y_start;
    int x2 = frame_buff->x_start + frame_buff->width;
    int y2 = frame_buff->y_start + frame_buff->height;

    /* Single copy of the whole region straight from VRAM into the panel's
     * internal frame buffer. The DPI panel accepts the PSRAM source and handles
     * any sub-window stride; CPU vs DMA2D is chosen by the panel's use_dma2d. */
    esp_err_t err = esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)dev->panel,
                                              x1, y1, x2, y2, frame_buff->frame);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "draw_bitmap failed: 0x%x", err);
        if (frame_buff->free_cb) {
            frame_buff->free_cb(frame_buff);
        }
        return OPRT_COM_ERROR;
    }

    /* draw_bitmap may copy asynchronously (DMA2D); wait for completion before
     * recycling the frame buffer so the caller (and any other 2D-DMA user) does
     * not race the copy. */
    if (dev->trans_done_sem) {
        xSemaphoreTake(dev->trans_done_sem, portMAX_DELAY);
    }

    if (frame_buff->free_cb) {
        frame_buff->free_cb(frame_buff);
    }

    return OPRT_OK;
}

static OPERATE_RET __esp_dpi_close(TDD_DISP_DEV_HANDLE_T device)
{
    DISP_ESP_DPI_DEV_T *dev = (DISP_ESP_DPI_DEV_T *)device;

    if (NULL == dev || NULL == dev->panel) {
        return OPRT_INVALID_PARM;
    }

    esp_lcd_panel_disp_on_off((esp_lcd_panel_handle_t)dev->panel, false);

    return OPRT_OK;
}

OPERATE_RET tdd_disp_esp_dpi_register(char *name, void *panel_io, void *panel,
                                      TDD_DISP_ESP_LCD_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == name || NULL == panel || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    DISP_ESP_DPI_DEV_T *dev = tal_malloc(sizeof(DISP_ESP_DPI_DEV_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(DISP_ESP_DPI_DEV_T));

    dev->panel_io = panel_io;
    dev->panel    = panel;
    memcpy(&dev->cfg, cfg, sizeof(TDD_DISP_ESP_LCD_CFG_T));

    dev->trans_done_sem = xSemaphoreCreateBinary();
    if (NULL == dev->trans_done_sem) {
        tal_free(dev);
        return OPRT_MALLOC_FAILED;
    }

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = __dpi_color_trans_done,
    };
    esp_err_t err = esp_lcd_dpi_panel_register_event_callbacks((esp_lcd_panel_handle_t)panel, &cbs, dev);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "register dpi event cb failed: 0x%x", err);
        vSemaphoreDelete(dev->trans_done_sem);
        tal_free(dev);
        return OPRT_COM_ERROR;
    }

    TDD_DISP_DEV_INFO_T dev_info = {
        .type     = TUYA_DISPLAY_MIPI_DSI,
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
        .open  = __esp_dpi_open,
        .flush = __esp_dpi_flush,
        .close = __esp_dpi_close,
    };

    rt = tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        vSemaphoreDelete(dev->trans_done_sem);
        tal_free(dev);
    }

    return rt;
}

/**
 * @file tuya_lvgl.c
 * @brief Initialize and manage LVGL library, related devices, and threads
 *
 * This source file provides the implementation for initializing the LVGL library,
 * registering LCD devices, setting up display and input devices, creating and
 * managing a mutex for thread synchronization, and starting a task for LVGL
 * operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_lvgl.h"

#include "board_config.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "tuya_lvgl"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Initialize LVGL library and related devices and threads
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (0 != board_display_init()) {
        return OPRT_COM_ERROR;
    }

    esp_lcd_panel_io_handle_t panel_io = (esp_lcd_panel_io_handle_t)board_display_get_panel_io_handle();
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)board_display_get_panel_handle();
    if (NULL == panel_io || NULL == panel) {
        return OPRT_COM_ERROR;
    }

    // Initialize LVGL port
    if (esp_lcd_panel_init(panel) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return OPRT_COM_ERROR;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = panel_io,
        .panel_handle = panel,
        .control_handle = NULL,
        .buffer_size = DISPLAY_BUFFER_SIZE,
        .double_buffer = false,
        .trans_size = 0,
        .hres = DISPLAY_WIDTH,
        .vres = DISPLAY_HEIGHT,
        .monochrome = DISPLAY_MONOCHROME,
        .rotation =
            {
                .swap_xy = DISPLAY_SWAP_XY,
                .mirror_x = DISPLAY_MIRROR_X,
                .mirror_y = DISPLAY_MIRROR_Y,
            },
        .color_format = DISPLAY_COLOR_FORMAT,
        .flags =
            {
                .buff_dma = DISPLAY_BUFF_DMA,
                .buff_spiram = 1,
                .sw_rotate = 0,
                .swap_bytes = DISPLAY_SWAP_BYTES,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to add display");
        return OPRT_COM_ERROR;
    }
    ESP_LOGI(TAG, "LVGL display added successfully");

    return rt;
}

/**
 * @brief Lock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_lock(void)
{
    OPERATE_RET rt = OPRT_OK;

    lvgl_port_lock(0);

    return rt;
}

/**
 * @brief Unlock the LVGL mutex
 *
 * @param None
 * @return OPERATE_RET Lock result, OPRT_OK indicates success
 */
OPERATE_RET tuya_lvgl_mutex_unlock(void)
{
    OPERATE_RET rt = OPRT_OK;

    lvgl_port_unlock();

    return rt;
}

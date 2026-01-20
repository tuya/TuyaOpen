/**
 * @file lv_port_disp.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include "lv_port_disp.h"
#include "board_config.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_SH8601)
#include "lcd_sh8601.h"
#endif
/*********************
 *      DEFINES
 *********************/
#define TAG "esp32_lvgl"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(char *device)
{
    if (0 != board_display_init()) {
        return;
    }

    esp_lcd_panel_io_handle_t panel_io = (esp_lcd_panel_io_handle_t)board_display_get_panel_io_handle();
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)board_display_get_panel_handle();
    if (NULL == panel_io || NULL == panel) {
        return;
    }

    // Initialize LVGL port
    if (esp_lcd_panel_init(panel) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

#if defined(BOARD_DISPLAY_TYPE) && (BOARD_DISPLAY_TYPE == DISPLAY_TYPE_LCD_SH8601)
#ifndef BOARD_LCD_DEFAULT_BRIGHTNESS
#define BOARD_LCD_DEFAULT_BRIGHTNESS 80
#endif
    lcd_sh8601_set_backlight(BOARD_LCD_DEFAULT_BRIGHTNESS);
#endif

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 8 * 1024;
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
                .buff_spiram = DISPLAY_BUFF_SPIRAM,
                .sw_rotate = 0,
                .swap_bytes = DISPLAY_SWAP_BYTES,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }
    ESP_LOGI(TAG, "LVGL display added successfully");
}

static void disp_deinit(void)
{

}

void lv_port_disp_deinit(void)
{
    lv_display_delete(lv_disp_get_default());
    disp_deinit();
}
#endif
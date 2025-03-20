/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "tuya_iot_config.h"
#include "lv_port_disp.h"
#include <stdbool.h>
#include "tal_log.h"

#include "tuya_lcd_device.h"
#include "tkl_display.h"
#include "tkl_memory.h"

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(TKL_DISP_DEVICE_S *device);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**********************
 *  STATIC VARIABLES
 **********************/
static TKL_DISP_DEVICE_S sg_lcd;
static TKL_DISP_INFO_S sg_lcd_info;
static lv_display_t *disp_drv_backup = NULL;

/**********************
 *      MACROS
 **********************/


/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(TKL_DISP_DEVICE_S *device)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init(device);

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t *disp = lv_display_create(sg_lcd_info.width, sg_lcd_info.height);
    lv_display_set_flush_cb(disp, disp_flush);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    uint32_t buf_len = sg_lcd_info.width*sg_lcd_info.height*BYTE_PER_PIXEL / 20;

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t *buf_2_1;
    buf_2_1 = (uint8_t*)LV_MEM_CUSTOM_MALLOC(buf_len);
    if(buf_2_1 == NULL) {
        PR_ERR("malloc failed");
        return;
    }
    memset(buf_2_1, 0x00, buf_len);

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t *buf_2_2;
    buf_2_2 = (uint8_t*)LV_MEM_CUSTOM_MALLOC(buf_len);
    if(buf_2_2 == NULL) {
        PR_ERR("malloc failed");
        return;
    }
    memset(buf_2_2, 0x00, buf_len);

    lv_display_set_buffers(disp, buf_2_1, buf_2_2, buf_len, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(TKL_DISP_DEVICE_S *device)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DISP_RECT_S rect;
    TKL_DISP_COLOR_U color;
    int brightness;

    memset(&sg_lcd, 0, sizeof(TKL_DISP_DEVICE_S));
    memset(&sg_lcd_info, 0, sizeof(TKL_DISP_INFO_S));

    sg_lcd.device_id   = device->device_id;
    sg_lcd.device_port = device->device_port;
    TUYA_CALL_ERR_RETURN(tkl_disp_init(&sg_lcd, NULL));
    memcpy(device, &sg_lcd, sizeof(TKL_DISP_DEVICE_S));

    TUYA_CALL_ERR_RETURN(tkl_disp_get_info(&sg_lcd, &sg_lcd_info));

    rect.x = 0;
    rect.y = 0;
    rect.width  = sg_lcd_info.width;
    rect.height = sg_lcd_info.height;

    color.full = 0xFFFFFFFF;

    tkl_disp_fill(&sg_lcd, &rect, color);

    tkl_disp_set_brightness(&sg_lcd, 100);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DISP_FRAMEBUFFER_S buf;
    TKL_DISP_RECT_S rect;
    
    if (disp_flush_enabled) {
        buf.buffer = (void *)px_map;
        buf.format = TKL_DISP_PIXEL_FMT_RGB565;
        rect.x = area->x1;
        rect.y = area->y1;
        rect.width = area->x2 - area->x1 + 1;
        rect.height = area->y2 - area->y1 + 1;

        tkl_disp_blit(&sg_lcd, &buf, &rect);

        if (lv_disp_flush_is_last(disp_drv)) {
            tkl_disp_flush(&sg_lcd);  
        }
    }

    lv_disp_flush_ready(disp_drv);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif

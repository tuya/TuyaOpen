/**
 * @file lv_vendor.c
 *
 */
#include "tuya_cloud_types.h"

#include "lv_vendor.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "board_config.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "esp32_lvgl"
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static uint8_t lvgl_task_state = STATE_INIT;
static bool lv_vendor_initialized = false;

void lv_vendor_disp_lock(void)
{
    lvgl_port_lock(0);
}

void lv_vendor_disp_unlock(void)
{
    lvgl_port_unlock();
}

void lv_vendor_init(void *device)
{
    if (lv_vendor_initialized) {
        LV_LOG_INFO("%s already init\n", __func__);
        return;
    }

    lv_init();

    lv_port_disp_init(device);

    lv_port_indev_init(device);

    lv_vendor_initialized = true;

    LV_LOG_INFO("%s complete\n", __func__);
}


void lv_vendor_start(uint32_t lvgl_task_pri, uint32_t lvgl_stack_size)
{
    if (lvgl_task_state == STATE_RUNNING) {
        LV_LOG_INFO("%s already start\n", __func__);
        return; 
    }

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = lvgl_stack_size;
    port_cfg.task_priority = lvgl_task_pri;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    LV_LOG_INFO("%s complete\n", __func__);
}

void lv_vendor_stop(void)
{
    if (lvgl_task_state == STATE_STOP) {
        LV_LOG_INFO("%s already stop\n", __func__);
        return;
    }

    lvgl_task_state = STATE_STOP;

    LV_LOG_INFO("%s complete\n", __func__);
}



// Modified by TUYA Start
void __attribute__((weak)) tuya_app_gui_feed_watchdog(void)
{

}

void __attribute__((weak)) lvMsgHandle(void)
{
}

void __attribute__((weak)) lvMsgEventReg(lv_obj_t *obj, lv_event_code_t eventCode)
{
}

void __attribute__((weak)) lvMsgEventDel(lv_obj_t *obj)
{
}
// Modified by TUYA End

#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_vendor.h"

#include "tal_api.h"
#include "tkl_thread.h"

static TKL_THREAD_HANDLE g_disp_thread_handle = NULL;
static MUTEX_HANDLE g_disp_mutex = NULL;
static SEM_HANDLE lvgl_sem = NULL;
static uint8_t lvgl_task_state = STATE_INIT;
static bool lv_vendor_initialized = false;

static uint32_t lv_tick_get_callback(void)
{
    return (uint32_t)tal_system_get_millisecond();
}

void lv_vendor_disp_lock(void)
{
    tal_mutex_lock(g_disp_mutex);
}

void lv_vendor_disp_unlock(void)
{
    tal_mutex_unlock(g_disp_mutex);
}

void lv_vendor_init(void *device)
{
    if (lv_vendor_initialized) {
        PR_NOTICE("%s already init\n", __func__);
        return;
    }

    lv_init();

    lv_port_disp_init((char *)device);

    lv_port_indev_init((char *)device);

    lv_tick_set_cb(lv_tick_get_callback);

    if (OPRT_OK != tal_mutex_create_init(&g_disp_mutex)) {
        PR_ERR("%s g_disp_mutex init failed\n", __func__);
        return;
    }

    if (OPRT_OK != tal_semaphore_create_init(&lvgl_sem, 0, 1)) {
        PR_ERR("%s semaphore init failed\n", __func__);
        return;
    }

    lv_vendor_initialized = true;

    LV_LOG_INFO("%s complete\n", __func__);
}

static void lv_tast_entry(void *arg)
{
    uint32_t sleep_time;

    lvgl_task_state = STATE_RUNNING;

    tal_semaphore_post(lvgl_sem);

    while(lvgl_task_state == STATE_RUNNING) {
        lv_vendor_disp_lock();
        sleep_time = lv_task_handler();
        lv_vendor_disp_unlock();

        #if CONFIG_LVGL_TASK_SLEEP_TIME_CUSTOMIZE
            sleep_time = CONFIG_LVGL_TASK_SLEEP_TIME;
        #else
            if (sleep_time > 500) {
                sleep_time = 500;
            } else if (sleep_time < 4) {
                sleep_time = 4;
            }
        #endif

        tal_system_sleep(sleep_time);
        // Modified by TUYA Start
        extern void tuya_app_gui_feed_watchdog(void);
        tuya_app_gui_feed_watchdog();
        // Modified by TUYA End
    }

    tal_semaphore_post(lvgl_sem);

    tkl_thread_release(g_disp_thread_handle);
    g_disp_thread_handle = NULL;
}

void lv_vendor_start(uint32_t lvgl_task_pri, uint32_t lvgl_stack_size)
{
    if (lvgl_task_state == STATE_RUNNING) {
        PR_NOTICE("%s already start\n", __func__);
        return; 
    }

#if defined(ENABLE_SMP) && (ENABLE_SMP == 1)
    if(OPRT_OK != tkl_thread_smp_create(&g_disp_thread_handle, 0, "lvgl", lvgl_stack_size, lvgl_task_pri, lv_tast_entry, NULL)) {
        LV_LOG_ERROR("%s lvgl task create failed\n", __func__);
        return;
    }
#else 
    if(OPRT_OK != tkl_thread_create(&g_disp_thread_handle, "lvgl_v9", lvgl_stack_size, lvgl_task_pri, lv_tast_entry, NULL)) {
        LV_LOG_ERROR("%s lvgl task create failed\n", __func__);
        return;
    }
#endif

    tal_semaphore_wait(lvgl_sem, SEM_WAIT_FOREVER);

    PR_NOTICE("%s complete\n", __func__);
}

void lv_vendor_stop(void)
{
    if (lvgl_task_state == STATE_STOP) {
        PR_NOTICE("%s already stop\n", __func__);
        return;
    }

    lvgl_task_state = STATE_STOP;

    tal_semaphore_wait(lvgl_sem, SEM_WAIT_FOREVER);

    PR_NOTICE("%s complete\n", __func__);
}

void lv_vendor_add_disp_dev(void *device)
{
    if(false == lv_vendor_initialized) {
        PR_ERR("%s lv vendor not init\n", __func__);
        return;
    }

    lv_port_disp_init((char *)device);

    lv_port_indev_init((char *)device);

    lv_display_t *lv_display = lv_port_get_lv_disp_by_name((char *)device);
    lv_indev_t *lv_indev = lv_port_get_lv_indev_by_name((char *)device);

    if(lv_display && lv_indev) {
        lv_indev_set_display(lv_indev, lv_display);
    }

    PR_NOTICE("add display device:%s complete\n", device);
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

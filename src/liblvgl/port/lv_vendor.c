#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_vendor.h"

#include "tuya_cloud_types.h"
#include "tkl_system.h"
#include "tkl_thread.h"
#include "tkl_mutex.h"
#include "tkl_semaphore.h"

static TKL_THREAD_HANDLE g_disp_thread_handle = NULL;
static TKL_MUTEX_HANDLE g_disp_mutex = NULL;
static TKL_SEM_HANDLE lvgl_sem = NULL;
static uint8_t lvgl_task_state = STATE_INIT;
static bool lv_vendor_initialized = false;

static uint32_t lv_tick_get_callback(void)
{
    return (uint32_t)tkl_system_get_millisecond();
}

void lv_vendor_disp_lock(void)
{
    tkl_mutex_lock(g_disp_mutex);
}

void lv_vendor_disp_unlock(void)
{
    tkl_mutex_unlock(g_disp_mutex);
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

    lv_tick_set_cb(lv_tick_get_callback);

    if (OPRT_OK != tkl_mutex_create_init(&g_disp_mutex)) {
        LV_LOG_ERROR("%s g_disp_mutex init failed\n", __func__);
        return;
    }

    if (OPRT_OK != tkl_semaphore_create_init(&lvgl_sem, 0, 1)) {
        LV_LOG_ERROR("%s semaphore init failed\n", __func__);
        return;
    }

    lv_vendor_initialized = true;

    LV_LOG_INFO("%s complete\n", __func__);
}

static void lv_tast_entry(void *arg)
{
    uint32_t sleep_time;

    lvgl_task_state = STATE_RUNNING;

    tkl_semaphore_post(lvgl_sem);

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

        tkl_system_sleep(sleep_time);

    }

    tkl_semaphore_post(lvgl_sem);

    tkl_thread_release(g_disp_thread_handle);
    g_disp_thread_handle = NULL;
}

void lv_vendor_start(void)
{
    if (lvgl_task_state == STATE_RUNNING) {
        LV_LOG_INFO("%s already start\n", __func__);
        return;
    }

    if(OPRT_OK != tkl_thread_create(&g_disp_thread_handle, "lvgl_v9", (1024 * 4), 4, lv_tast_entry, NULL)) {
        LV_LOG_ERROR("%s lvgl task create failed\n", __func__);
        return;
    }

    tkl_semaphore_wait(lvgl_sem, TKL_SEM_WAIT_FOREVER);

    LV_LOG_INFO("%s complete\n", __func__);
}

void lv_vendor_stop(void)
{
    if (lvgl_task_state == STATE_STOP) {
        LV_LOG_INFO("%s already stop\n", __func__);
        return;
    }

    lvgl_task_state = STATE_STOP;

    tkl_semaphore_wait(lvgl_sem, TKL_SEM_WAIT_FOREVER);


    LV_LOG_INFO("%s complete\n", __func__);
}


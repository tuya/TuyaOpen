/**
 * @file tdl_led_manage.c
 * @brief LED management implementation for TDL layer
 *
 * This file implements the LED management functions for the TDL (Tuya Device Library) layer.
 * It provides device registration, discovery, state management, timer-based blinking/flashing,
 * and thread-safe operations for controlling LED devices through a unified interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tdl_led_driver.h"
#include "tdl_led_manage.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
// Number of brightness steps per breath half-cycle (dim->bright). Higher = smoother
// but more timer ticks.
#define LED_BREATH_STEPS 50

typedef enum {
    LED_BLINK_IDLE,
    LED_BLINK_START,
    LED_BLINK_FIRST,
    LED_BLINK_LATTER,
} LED_BLINK_STAT_E;

typedef struct {
    struct tuya_list_head node;
    char                  name[LED_DEV_NAME_MAX_LEN + 1];
    MUTEX_HANDLE          mutex;

    TDD_LED_HANDLE_T *drv_hdl;
    TDD_LED_INTFS_T   drv_intfs;

    TDL_LED_MODE_E mode;
    bool           is_open;
    bool           is_on;

    TIMER_ID led_tm;

    LED_BLINK_STAT_E    blink_stat;
    uint32_t            blink_cnt;
    TDL_LED_BLINK_CFG_T blink_cfg;

    TDL_LED_BREATH_CFG_T breath_cfg;
    uint32_t             breath_cnt;       // remaining breath cycles
    uint16_t             breath_idx;       // current step index, 0..LED_BREATH_STEPS
    int8_t               breath_dir;       // +1 ramping up, -1 ramping down
    uint32_t             breath_step_time; // ms between steps
} LED_DEV_INFO_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static struct tuya_list_head sg_led_list = LIST_HEAD_INIT(sg_led_list);

/***********************************************************
***********************function define**********************
***********************************************************/
static LED_DEV_INFO_T *__find_led_device(char *name)
{
    LED_DEV_INFO_T        *led_dev = NULL;
    struct tuya_list_head *pos     = NULL;

    if (NULL == name) {
        return NULL;
    }

    tuya_list_for_each(pos, &sg_led_list)
    {
        led_dev = tuya_list_entry(pos, LED_DEV_INFO_T, node);
        if (0 == strncmp(led_dev->name, name, LED_DEV_NAME_MAX_LEN)) {
            return led_dev;
        }
    }

    return NULL;
}

static OPERATE_RET __led_set_status(LED_DEV_INFO_T *led_dev, TDL_LED_STATUS_E status)
{
    OPERATE_RET rt    = OPRT_OK;
    bool        is_on = false;

    if (NULL == led_dev) {
        return OPRT_INVALID_PARM;
    }

    if (TDL_LED_OFF == status) {
        is_on = false;
    } else if (TDL_LED_ON == status) {
        is_on = true;
    } else {
        is_on = !led_dev->is_on;
    }

    if (led_dev->drv_intfs.led_set) {
        TUYA_CALL_ERR_LOG(led_dev->drv_intfs.led_set(led_dev->drv_hdl, is_on));
        if (rt != OPRT_OK) {
            return rt;
        }
    }
    led_dev->is_on = is_on;

    return OPRT_OK;
}

static void __led_blink_handle(LED_DEV_INFO_T *led_dev)
{
    uint32_t nxt_time = 0;

    if (NULL == led_dev) {
        return;
    }

    switch (led_dev->blink_stat) {
    case LED_BLINK_START:
        __led_set_status(led_dev, led_dev->blink_cfg.start_stat);
        led_dev->blink_stat = LED_BLINK_FIRST;
        nxt_time            = led_dev->blink_cfg.first_half_cycle_time;
        break;
    case LED_BLINK_FIRST:
        __led_set_status(led_dev, TDL_LED_TOGGLE);
        led_dev->blink_stat = LED_BLINK_LATTER;
        nxt_time            = led_dev->blink_cfg.latter_half_cycle_time;
        break;
    case LED_BLINK_LATTER:
        if (led_dev->blink_cnt > 0 && led_dev->blink_cnt != TDL_BLINK_FOREVER) {
            led_dev->blink_cnt--;
        }

        if (0 == led_dev->blink_cnt) {
            __led_set_status(led_dev, led_dev->blink_cfg.end_stat);
            led_dev->blink_stat = LED_BLINK_IDLE;
            nxt_time            = 0;
        } else {
            __led_set_status(led_dev, TDL_LED_TOGGLE);
            led_dev->blink_stat = LED_BLINK_FIRST;
            nxt_time            = led_dev->blink_cfg.first_half_cycle_time;
        }
        break;
    default:
        break;
    }

    if (nxt_time) {
        tal_sw_timer_start(led_dev->led_tm, nxt_time, TAL_TIMER_ONCE);
    }

    return;
}

static void __led_breath_handle(LED_DEV_INFO_T *led_dev)
{
    uint8_t  min = 0, max = 0, level = 0;
    uint32_t range = 0;
    int      next  = 0;

    if (NULL == led_dev || NULL == led_dev->drv_intfs.led_set_level) {
        return;
    }

    min   = led_dev->breath_cfg.min_level;
    max   = led_dev->breath_cfg.max_level;
    range = (max > min) ? (uint32_t)(max - min) : 0;

    // Output the level for the current step, then step toward the next.
    level = (uint8_t)(min + range * led_dev->breath_idx / LED_BREATH_STEPS);
    led_dev->drv_intfs.led_set_level(led_dev->drv_hdl, level);
    led_dev->is_on = (level > 0);

    next = (int)led_dev->breath_idx + led_dev->breath_dir;
    if (next >= LED_BREATH_STEPS) {
        next                = LED_BREATH_STEPS;
        led_dev->breath_dir = -1;
    } else if (next <= 0) {
        next                = 0;
        led_dev->breath_dir = 1;
        // Back at the dim end => one full breath cycle completed.
        if (led_dev->breath_cnt > 0 && led_dev->breath_cnt != TDL_BLINK_FOREVER) {
            led_dev->breath_cnt--;
            if (0 == led_dev->breath_cnt) {
                led_dev->drv_intfs.led_set_level(led_dev->drv_hdl, min);
                return; // done: stop the timer chain
            }
        }
    }
    led_dev->breath_idx = (uint16_t)next;

    tal_sw_timer_start(led_dev->led_tm, led_dev->breath_step_time, TAL_TIMER_ONCE);
}

static void __led_blink_timer_cb(TIMER_ID timerID, void *pTimerArg)
{
    LED_DEV_INFO_T *led_dev = (LED_DEV_INFO_T *)pTimerArg;

    if (NULL == led_dev) {
        return;
    }

    tal_mutex_lock(led_dev->mutex);

    if (TDL_LED_MODE_BREATH == led_dev->mode) {
        __led_breath_handle(led_dev);
    } else {
        __led_blink_handle(led_dev);
    }

    tal_mutex_unlock(led_dev->mutex);
}

static void __led_stop_blink(LED_DEV_INFO_T *led_dev)
{
    if (NULL == led_dev) {
        return;
    }

    if (tal_sw_timer_is_running(led_dev->led_tm)) {
        tal_sw_timer_stop(led_dev->led_tm);
    }
    led_dev->blink_stat = LED_BLINK_IDLE;
    led_dev->blink_cnt  = 0;
    // Also cancel any running breath effect (shares the same timer).
    led_dev->breath_cnt = 0;
}

/**
 * @brief Finds and returns a handle to the registered LED device by its name.
 *
 * @param dev_name The name of the LED device to find.
 *
 * @return Returns a handle to the LED device if found, otherwise NULL.
 */
TDL_LED_HANDLE_T tdl_led_find_dev(char *dev_name)
{
    LED_DEV_INFO_T *led_dev = NULL;

    led_dev = __find_led_device(dev_name);
    if (NULL == led_dev) {
        return NULL;
    }

    return (TDL_LED_HANDLE_T)led_dev;
}

/**
 * @brief Opens the LED device and initializes its resources.
 *
 * @param handle The handle to the LED device.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_open(TDL_LED_HANDLE_T handle)
{
    OPERATE_RET     rt      = OPRT_OK;
    LED_DEV_INFO_T *led_dev = NULL;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    led_dev = (LED_DEV_INFO_T *)handle;

    if (true == led_dev->is_open) {
        PR_NOTICE("led is already open");
        return OPRT_OK;
    }

    if (NULL == led_dev->mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&led_dev->mutex));
    }

    if (NULL == led_dev->led_tm) {
        TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__led_blink_timer_cb, handle, &led_dev->led_tm));
    }

    if (led_dev->drv_intfs.led_open) {
        TUYA_CALL_ERR_RETURN(led_dev->drv_intfs.led_open(led_dev->drv_hdl));
    }

    led_dev->is_open = true;

    return OPRT_OK;
}

/**
 * @brief Sets the LED device to a specified status (ON, OFF, or TOGGLE).
 *
 * @param handle The handle to the LED device.
 * @param status The desired status of the LED (TDL_LED_ON, TDL_LED_OFF, or TDL_LED_TOGGLE).
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_set_status(TDL_LED_HANDLE_T handle, TDL_LED_STATUS_E status)
{
    LED_DEV_INFO_T *led_dev = (LED_DEV_INFO_T *)handle;

    if (NULL == led_dev) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(led_dev->mutex);

    if (false == led_dev->is_open) {
        PR_ERR("led is not open");
        tal_mutex_unlock(led_dev->mutex);
        return OPRT_COM_ERROR;
    }

    __led_stop_blink(led_dev);

    __led_set_status(led_dev, status);
    led_dev->mode = led_dev->is_on ? TDL_LED_MODE_ON : TDL_LED_MODE_OFF;

    tal_mutex_unlock(led_dev->mutex);

    PR_NOTICE("led_set_status: %d", status);

    return OPRT_OK;
}

/**
 * @brief Starts the LED flashing with a specified half-cycle time.
 *
 * @param handle The handle to the LED device.
 * @param half_cycle_time The duration of each half cycle in milliseconds.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_flash(TDL_LED_HANDLE_T handle, uint32_t half_cycle_time)
{
    OPERATE_RET     rt      = OPRT_OK;
    LED_DEV_INFO_T *led_dev = (LED_DEV_INFO_T *)handle;

    if (NULL == led_dev || 0 == half_cycle_time) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(led_dev->mutex);

    if (false == led_dev->is_open) {
        PR_ERR("led is not open");
        tal_mutex_unlock(led_dev->mutex);
        return OPRT_COM_ERROR;
    }

    __led_stop_blink(led_dev);

    led_dev->blink_cfg.cnt                    = TDL_BLINK_FOREVER;
    led_dev->blink_cfg.start_stat             = TDL_LED_ON;
    led_dev->blink_cfg.first_half_cycle_time  = half_cycle_time;
    led_dev->blink_cfg.latter_half_cycle_time = half_cycle_time;

    led_dev->mode       = TDL_LED_MODE_FLASH;
    led_dev->blink_stat = LED_BLINK_START;
    led_dev->blink_cnt  = led_dev->blink_cfg.cnt;

    rt = tal_sw_timer_start(led_dev->led_tm, 10, TAL_TIMER_ONCE);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to trigger LED timer: %d", rt);
        tal_mutex_unlock(led_dev->mutex);
        return rt;
    }

    tal_mutex_unlock(led_dev->mutex);

    return OPRT_OK;
}

/**
 * @brief Starts the LED blinking with the specified configuration.
 *
 * @param handle The handle to the LED device.
 * @param cfg A pointer to the TDL_LED_BLINK_CFG_T structure containing the blink configuration.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_blink(TDL_LED_HANDLE_T handle, TDL_LED_BLINK_CFG_T *cfg)
{
    OPERATE_RET     rt      = OPRT_OK;
    LED_DEV_INFO_T *led_dev = (LED_DEV_INFO_T *)handle;

    if (NULL == led_dev || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(led_dev->mutex);

    if (false == led_dev->is_open) {
        PR_ERR("led is not open");
        tal_mutex_unlock(led_dev->mutex);
        return OPRT_COM_ERROR;
    }

    __led_stop_blink(led_dev);

    memcpy(&led_dev->blink_cfg, cfg, sizeof(TDL_LED_BLINK_CFG_T));

    led_dev->mode       = TDL_LED_MODE_BLINK;
    led_dev->blink_stat = LED_BLINK_START;
    led_dev->blink_cnt  = led_dev->blink_cfg.cnt;

    rt = tal_sw_timer_start(led_dev->led_tm, 10, TAL_TIMER_ONCE);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to trigger LED timer: %d", rt);
        tal_mutex_unlock(led_dev->mutex);
        return rt;
    }

    tal_mutex_unlock(led_dev->mutex);

    return OPRT_OK;
}

/**
 * @brief Starts a smooth breathing effect (brightness ramps dim<->bright).
 *
 * @param handle The handle to the LED device.
 * @param cfg A pointer to the TDL_LED_BREATH_CFG_T structure containing the breath configuration.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_breath(TDL_LED_HANDLE_T handle, TDL_LED_BREATH_CFG_T *cfg)
{
    OPERATE_RET     rt        = OPRT_OK;
    LED_DEV_INFO_T *led_dev   = (LED_DEV_INFO_T *)handle;
    uint32_t        step_time = 0;

    if (NULL == led_dev || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (0 == cfg->period_ms || cfg->max_level > 100 || cfg->min_level > cfg->max_level) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(led_dev->mutex);

    if (false == led_dev->is_open) {
        PR_ERR("led is not open");
        tal_mutex_unlock(led_dev->mutex);
        return OPRT_COM_ERROR;
    }

    // Breathing needs brightness control; on/off-only drivers (GPIO) cannot do it.
    if (NULL == led_dev->drv_intfs.led_set_level) {
        PR_ERR("led driver has no brightness control, breath not supported");
        tal_mutex_unlock(led_dev->mutex);
        return OPRT_NOT_SUPPORTED;
    }

    __led_stop_blink(led_dev);

    memcpy(&led_dev->breath_cfg, cfg, sizeof(TDL_LED_BREATH_CFG_T));
    led_dev->breath_cnt = cfg->cnt;
    led_dev->breath_idx = 0;
    led_dev->breath_dir = 1;

    // One full breath = 2 * LED_BREATH_STEPS ticks (dim->bright->dim); >=1 ms per tick.
    step_time = cfg->period_ms / (2 * LED_BREATH_STEPS);
    led_dev->breath_step_time = (0 == step_time) ? 1 : step_time;

    led_dev->mode = TDL_LED_MODE_BREATH;

    rt = tal_sw_timer_start(led_dev->led_tm, 10, TAL_TIMER_ONCE);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to trigger LED timer: %d", rt);
        tal_mutex_unlock(led_dev->mutex);
        return rt;
    }

    tal_mutex_unlock(led_dev->mutex);

    return OPRT_OK;
}

/**
 * @brief Closes the LED device and releases associated resources.
 *
 * @param handle A pointer to the LED device handle.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_close(TDL_LED_HANDLE_T handle)
{
    OPERATE_RET     rt      = OPRT_OK;
    LED_DEV_INFO_T *led_dev = (LED_DEV_INFO_T *)handle;

    if (NULL == led_dev) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(led_dev->mutex);

    if (false == led_dev->is_open) {
        PR_NOTICE("led is already close");
        tal_mutex_unlock(led_dev->mutex);
        return OPRT_OK;
    }

    __led_stop_blink(led_dev);

    if (led_dev->drv_intfs.led_close) {
        rt = led_dev->drv_intfs.led_close(led_dev->drv_hdl);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to close LED driver: %d", rt);
            tal_mutex_unlock(led_dev->mutex);
            return rt;
        }
    }

    led_dev->is_open = false;

    tal_mutex_unlock(led_dev->mutex);

    return OPRT_OK;
}

/**
 * @brief Registers an LED driver with the system.
 *
 * @param dev_name The name of the device to register.
 * @param handle the LED device handle.
 * @param p_intfs A pointer to the LED driver interface functions.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_driver_register(char *dev_name, TDD_LED_HANDLE_T handle, TDD_LED_INTFS_T *p_intfs)
{
    LED_DEV_INFO_T *led_dev = NULL;

    if (NULL == dev_name || NULL == handle || NULL == p_intfs) {
        return OPRT_INVALID_PARM;
    }

    NEW_LIST_NODE(LED_DEV_INFO_T, led_dev);
    if (NULL == led_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(led_dev, 0, sizeof(LED_DEV_INFO_T));

    strncpy(led_dev->name, dev_name, LED_DEV_NAME_MAX_LEN);
    memcpy(&led_dev->drv_intfs, p_intfs, sizeof(TDD_LED_INTFS_T));
    led_dev->drv_hdl = handle;

    tuya_list_add(&led_dev->node, &sg_led_list);

    return OPRT_OK;
}

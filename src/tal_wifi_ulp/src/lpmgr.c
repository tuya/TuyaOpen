/**
 * @file lpmgr.c
 * @brief lp manager - Ultra Low Power reference counting manager
 * @version 0.1
 * @date 2024-06-06
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lpmgr.h"
#include "tal_mutex.h"
#include "tal_sleep.h"
#include "tal_wifi.h"
#include "tuya_iot_config.h"
#include "tal_log.h"
#include "netmgr.h"
#include "netmgr_timer.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TUYA_SVC_TIMER_TASK_CHECK_MIN (30 * 1000) /* 30 secs */

/***********************************************************
*************************variable define********************
***********************************************************/

static MUTEX_HANDLE lp_mutex = NULL;
static lp_info_t lp_info_current;

static lp_info_t lp_info_map[TY_LP_MAX] = {
    { TY_LP_NETCFG,             OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
    { TY_LP_AP_REDUCE_CONN,     OS_LP_SLEEP_5S,     10,     MAX_TIMEOUT,        1 },
    { TY_LP_NET_REDUCE_CONN,    OS_LP_SLEEP_10S,    10,     MAX_TIMEOUT,        1 },
    { TY_LP_NETCONN,            OS_LP_CLOSE,        10,     3 * 60 * 1000,      1 },
    { TY_LP_TLSCONN,            OS_LP_SLEEP_3_5S,   3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP,               OS_LP_SLEEP_200ms,  10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP_CONN,          OS_LP_SLEEP_200ms,  10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP_SEND,          OS_LP_SLEEP_200ms,  10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT,               OS_LP_SLEEP_2S,     3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT_CONN,          OS_LP_SLEEP_3_5S,   3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT_PUBLISH,       OS_LP_SLEEP_3_5S,   10,     30 * 1000,          MAX_REGISTER_CNT },
    { TY_LP_MQTT_PING,          OS_LP_SLEEP_3_5S,   10,     60 * 1000,          MAX_REGISTER_CNT },
    { TY_LP_TCP_CONN,           OS_LP_SLEEP_3S,     10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_DNS,                OS_LP_SLEEP_1S,     10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_DHCP_RENEW,         OS_LP_CLOSE,        0,      5 * 1000,           MAX_REGISTER_CNT },
    { TY_LP_OTA,                OS_LP_CLOSE,        0,      10 * 60 * 1000,     1 },
    { TY_LP_UART,               OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_I2C,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_SPI,                OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
    { TY_LP_PIR,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_CRG,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_BTN,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_PWM,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_KEY,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_NORMAL,             OS_LP_SLEEP_MAX,    10,     MAX_TIMEOUT,        1 },
    { TY_LP_DEEP,               OS_LP_SLEEP_MAX,    10,     MAX_TIMEOUT,        1 },
    { TY_LP_APP_USED,           OS_LP_SLEEP_MAX,    10,     MAX_TIMEOUT,        1 },
    { TY_LP_BLE,                OS_LP_CLOSE,        10,     MAX_TIMEOUT,        1 },
    { TY_LP_LONG_NETCFG,        OS_LP_SLEEP_5S,     10,     10 * 60 * 1000,     1 },
    { TY_LP_DP,                 OS_LP_CLOSE,        10,     1 * 1000,           MAX_REGISTER_CNT },
    { TY_LP_DISABLE,            OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
};

static lp_info_t lp_dtim20_info_map[TY_LP_MAX] = {
    { TY_LP_NETCFG,             OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
    { TY_LP_AP_REDUCE_CONN,     OS_LP_SLEEP_5S,     0,      MAX_TIMEOUT,        1 },
    { TY_LP_NET_REDUCE_CONN,    OS_LP_SLEEP_10S,    20,     MAX_TIMEOUT,        1 },
    { TY_LP_NETCONN,            OS_LP_CLOSE,        20,     3 * 60 * 1000,      1 },
    { TY_LP_TLSCONN,            4100,               3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP,               4100,               20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP_CONN,          4100,               20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP_SEND,          4100,               20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT,               4100,               3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT_CONN,          4100,               3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT_PUBLISH,       4100,               20,     30 * 1000,          MAX_REGISTER_CNT },
    { TY_LP_MQTT_PING,          4100,               20,     60 * 1000,          MAX_REGISTER_CNT },
    { TY_LP_TCP_CONN,           4100,               20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_DNS,                2100,               20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_DHCP_RENEW,         OS_LP_CLOSE,        0,      5 * 1000,           MAX_REGISTER_CNT },
    { TY_LP_OTA,                OS_LP_CLOSE,        0,      10 * 60 * 1000,     1 },
    { TY_LP_UART,               OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_I2C,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_SPI,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        1 },
    { TY_LP_PIR,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_CRG,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_BTN,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_PWM,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_KEY,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_NORMAL,             OS_LP_SLEEP_MAX,    20,     MAX_TIMEOUT,        1 },
    { TY_LP_DEEP,               OS_LP_SLEEP_MAX,    20,     MAX_TIMEOUT,        1 },
    { TY_LP_APP_USED,           OS_LP_SLEEP_MAX,    20,     MAX_TIMEOUT,        1 },
    { TY_LP_BLE,                OS_LP_CLOSE,        20,     MAX_TIMEOUT,        1 },
    { TY_LP_LONG_NETCFG,        OS_LP_SLEEP_5S,     20,     10 * 60 * 1000,     1 },
    { TY_LP_DP,                 OS_LP_CLOSE,        20,     1 * 1000,           MAX_REGISTER_CNT },
    { TY_LP_DISABLE,            OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
};

static lp_info_t lp_dtim30_info_map[TY_LP_MAX] = {
    { TY_LP_NETCFG,             OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
    { TY_LP_AP_REDUCE_CONN,     OS_LP_SLEEP_5S,     0,      MAX_TIMEOUT,        1 },
    { TY_LP_NET_REDUCE_CONN,    OS_LP_SLEEP_10S,    30,     MAX_TIMEOUT,        1 },
    { TY_LP_NETCONN,            OS_LP_CLOSE,        30,     3 * 60 * 1000,      1 },
    { TY_LP_TLSCONN,            6100,               3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP,               6100,               30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP_CONN,          6100,               30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_HTTP_SEND,          6100,               30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT,               6100,               3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT_CONN,          6100,               3,      MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_MQTT_PUBLISH,       6100,               30,     30 * 1000,          MAX_REGISTER_CNT },
    { TY_LP_MQTT_PING,          6100,               30,     60 * 1000,          MAX_REGISTER_CNT },
    { TY_LP_TCP_CONN,           6100,               30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_DNS,                3100,               30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_DHCP_RENEW,         OS_LP_CLOSE,        0,      5 * 1000,           MAX_REGISTER_CNT },
    { TY_LP_OTA,                OS_LP_CLOSE,        0,      10 * 60 * 1000,     1 },
    { TY_LP_UART,               OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_I2C,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_SPI,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        1 },
    { TY_LP_PIR,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_CRG,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_BTN,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_PWM,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_KEY,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        MAX_REGISTER_CNT },
    { TY_LP_NORMAL,             OS_LP_SLEEP_MAX,    30,     MAX_TIMEOUT,        1 },
    { TY_LP_DEEP,               OS_LP_SLEEP_MAX,    30,     MAX_TIMEOUT,        1 },
    { TY_LP_APP_USED,           OS_LP_SLEEP_MAX,    30,     MAX_TIMEOUT,        1 },
    { TY_LP_BLE,                OS_LP_CLOSE,        30,     MAX_TIMEOUT,        1 },
    { TY_LP_LONG_NETCFG,        OS_LP_SLEEP_5S,     30,     10 * 60 * 1000,     1 },
    { TY_LP_DP,                 OS_LP_CLOSE,        30,     1 * 1000,           MAX_REGISTER_CNT },
    { TY_LP_DISABLE,            OS_LP_CLOSE,        0,      MAX_TIMEOUT,        1 },
};

static lp_info_t *g_lp_info_map = lp_info_map;

uint32_t g_cpu_sleep_time_ms = 0;
extern struct netmgr_timer lptimer;
static unsigned char defmin_dtim = TY_LP_DITM_10;
static int32_t enforce = 0;

/***********************************************************
*************************function define********************
***********************************************************/

/**
 * @brief Set CPU low power state (non-counting mode)
 * Uses wakelock bitmap, same as TuyaOS: acquire(0) to keep awake, release(0) to allow sleep
 */
static int lpmgr_set_cpu_lp(bool enable)
{
    if (enable) {
        return tal_cpu_release_wakelock(0);
    }
    return tal_cpu_acquire_wakelock(0);
}

/**
 * @brief Set WiFi low power state (non-counting mode)
 */
static int lpmgr_set_wifi_lp(bool enable)
{
    static bool lp_disable = false;
    int ret = OPRT_OK;
    if (enable) {
        ret = tal_wifi_lp_enable();
        lp_disable = false;
    } else {
        if (lp_disable) {
            return ret;
        }
        ret = tal_wifi_lp_disable();
        lp_disable = true;
    }
    return ret;
}

static void lp_timer_handler(struct netmgr_timer* timer);
static uint32_t lpmgr_min_timeout(void)
{
    uint64_t min_timeout_point = LONG_MAX_TIMEOUT;
    uint64_t current_time = current_timestamp();
    int i = 0;
    lp_info_t *lp_info_p = g_lp_info_map;

    while (TY_LP_MAX > i) {
        if (lp_info_p->timeout_point > current_time && lp_info_p->timeout_point < min_timeout_point) {
            min_timeout_point = lp_info_p->timeout_point;
        }
        i++;
        lp_info_p++;
    }

    if (LONG_MAX_TIMEOUT == min_timeout_point) {
        return 0;
    } else {
        return min_timeout_point - current_time;
    }
}

static int lpmgr_set_mcu_power_type(void)
{
    int ret = 0;
    int retry_count = 3;
    int i = 0;
    int min_mcu_power = OS_LP_SLEEP_MAX;
    uint64_t current_time = current_timestamp();
    lp_info_t *lp_info_p = g_lp_info_map;

retry:
    PR_TRACE("lpmgr_set_mcu_power_type start");

    while (TY_LP_MAX > i) {
        if (lp_info_p->timeout_point > current_time) {
            if (lp_info_p->mcu_lp_type < min_mcu_power) {
                min_mcu_power = lp_info_p->mcu_lp_type;
                PR_TRACE("lp_info_p[%d].mcu_lp_type:%d", i, lp_info_p->mcu_lp_type);
            }
        }
        i++;
        lp_info_p++;
    }

    PR_TRACE("enforce:%d, min_mcu_power:%d", enforce, min_mcu_power);
    if (!enforce && min_mcu_power != lp_info_current.mcu_lp_type) {
        if (min_mcu_power == OS_LP_CLOSE) {
            ret = lpmgr_set_cpu_lp(false);
            PR_TRACE("ULP: CPU sleep disabled (keep awake)");
        } else {
            lpmgr_set_cpu_fix_sleep_time(min_mcu_power);
            ret = lpmgr_set_cpu_lp(true);
            PR_NOTICE("ULP: CPU sleep changed %d -> %dms", lp_info_current.mcu_lp_type, min_mcu_power);
        }
    }

    if (0 == ret) {
        lp_info_current.mcu_lp_type = min_mcu_power;
    }

    if (0 != ret && retry_count > 0) {
        PR_ERR("retry_count:%d", retry_count);
        retry_count--;
        goto retry;
    }

    PR_TRACE("lpmgr_set_mcu_power_type end");

    return ret;
}

static int lpmgr_set_wifi_dtim(void)
{
    int ret = 0;
    int retry_count = 3;
    int i = 0;
    char min_dtim = defmin_dtim;
    uint64_t current_time = current_timestamp();
    lp_info_t *lp_info_p = g_lp_info_map;

retry:
    PR_TRACE("lpmgr_set_wifi_dtim start");

    while (TY_LP_MAX > i) {
        if (lp_info_p->timeout_point > current_time) {
            if (lp_info_p->dtim < min_dtim) {
                min_dtim = lp_info_p->dtim;
                PR_TRACE("lp_info_p[%d].dtim:%d", i, lp_info_p->dtim);
            }
        }
        i++;
        lp_info_p++;
    }

    PR_TRACE("enforce:%d, min_dtim:%d, defmin_dtim:%d, dtim:%d", enforce, min_dtim, defmin_dtim, lp_info_current.dtim);
    if (!enforce && min_dtim != lp_info_current.dtim) {
        if (min_dtim != 0) {
            lpmgr_set_wifi_lp(false);
            tal_wifi_set_lps_dtim(min_dtim);
            lpmgr_set_wifi_lp(true);
            PR_NOTICE("ULP: WiFi DTIM changed %d -> %d", lp_info_current.dtim, min_dtim);
        } else {
            lpmgr_set_wifi_lp(false);
            PR_TRACE("ULP: WiFi LP disabled (dtim=0)");
        }
        ret = 0;
    }

    if (0 == ret) {
        lp_info_current.dtim = min_dtim;
    }

    if (0 != ret && retry_count > 0) {
        PR_ERR("retry_count:%d", retry_count);
        retry_count--;
        goto retry;
    }

    PR_TRACE("lpmgr_set_wifi_dtim end");

    return ret;
}

static int lpmgr_set_power_mode(void)
{
    int ret = 0;

    PR_TRACE("lpmgr_set_power_mode start");

    ret = lpmgr_set_mcu_power_type();
    if (0 != ret) {
        PR_ERR("failed in lpmgr_set_mcu_power_type, exit!");
        goto exit;
    }

    ret = lpmgr_set_wifi_dtim();
    if (0 != ret) {
        PR_ERR("failed in lpmgr_set_wifi_dtim, exit!");
        goto exit;
    }

exit:
    netmgr_set_timer(&lptimer, lpmgr_min_timeout(), lp_timer_handler);
    netmgr_timer_refresh();

    PR_TRACE("lpmgr_set_power_mode end");

    return ret;
}

static void lp_timer_handler(struct netmgr_timer* timer)
{
    PR_TRACE("lp_timer_handler start");
    tal_mutex_lock(lp_mutex);
    lpmgr_set_power_mode();
    tal_mutex_unlock(lp_mutex);
    PR_TRACE("lp_timer_handler end");
    return;
}

int lpmgr_unregister(TY_LP_TYPE type)
{
    int i = 0;
    int ret = 0;
    lp_info_t* lp_info_p = NULL;
    lp_info_t *ptr = g_lp_info_map;

    PR_TRACE("ULP: unregister type=%d", type);
    tal_mutex_lock(lp_mutex);
    for (i = 0; i < TY_LP_MAX; i++) {
        if (type == ptr->type) {
            lp_info_p = ptr;
            break;
        }
        ptr++;
    }

    if (NULL == lp_info_p) {
        PR_ERR("Invalid param type:%d i:%d lp_info_map:%p ", type, i, lp_info_p);
        ret = -1;
        goto exit;
    }

    PR_TRACE("cnt = %d", lp_info_p->cnt);

    if (lp_info_p->cnt > 0) {
        lp_info_p->cnt--;
        if (lp_info_p->cnt == 0) {
            lp_info_p->timeout_point = 0;
        } else {
            goto exit;
        }
    } else {
        goto exit;
    }

    lpmgr_set_power_mode();

exit:
    tal_mutex_unlock(lp_mutex);

    PR_TRACE("lpmgr_unregister end");

    return ret;
}

int lpmgr_is_registered(TY_LP_TYPE type)
{
    int i = 0;
    int ret = 0;
    lp_info_t* lp_info_p = NULL;
    lp_info_t *ptr = g_lp_info_map;

    PR_INFO("lpmgr_register start,type = %d", type);
    tal_mutex_lock(lp_mutex);
    for (i = 0; i < TY_LP_MAX; i++) {
        if (type == ptr->type) {
            lp_info_p = ptr;
            if (lp_info_p->timeout_point <= current_timestamp()) {
                lp_info_p->cnt = 0;
            }
            break;
        }
        ptr++;
    }

    if (NULL != lp_info_p && 0 != lp_info_p->cnt) {
        ret = 1;
    }
    tal_mutex_unlock(lp_mutex);

    return ret;
}

int lpmgr_register(TY_LP_TYPE type)
{
    int i = 0;
    int ret = 0;
    lp_info_t* lp_info_p = NULL;
    lp_info_t *ptr = g_lp_info_map;

    PR_TRACE("ULP: register type=%d", type);
    tal_mutex_lock(lp_mutex);
    for (i = 0; i < TY_LP_MAX; i++) {
        if (type == ptr->type) {
            lp_info_p = ptr;
            if (lp_info_p->timeout_point <= current_timestamp()) {
                lp_info_p->cnt = 0;
            }
            lp_info_p->cnt++;
            PR_TRACE("register cnt: %d", lp_info_p->cnt > lp_info_p->max_cnt ? lp_info_p->max_cnt : lp_info_p->cnt);
            if (lp_info_p->cnt > lp_info_p->max_cnt) {
                lp_info_p->cnt = lp_info_p->max_cnt;
            }
            break;
        }
        ptr++;
    }

    if (NULL == lp_info_p) {
        PR_ERR("Invalid param type:%d i:%d lp_info_p:%p ", type, i, lp_info_p);
        ret = -1;
        goto exit;
    }

    lp_info_p->timeout_point = (lp_info_p->timeout_range != MAX_TIMEOUT) ? (lp_info_p->timeout_range + current_timestamp()) : LONG_MAX_TIMEOUT;

    lpmgr_set_power_mode();

exit:
    tal_mutex_unlock(lp_mutex);

    PR_TRACE("lpmgr_register end");

    return ret;
}

static void lpmgr_set_lp_info_map(user_set_map_t* lp_set_info_map)
{
    PR_TRACE("lpmgr_set_lp_info_map start\n");
    int num = 0;
    int i = 0;
    int j = 0;
    lp_info_t* lp_info_p = NULL;
    lp_info_t *ptr = g_lp_info_map;

    if (lp_set_info_map == NULL) {
        PR_TRACE("no user map");
        return;
    }
    tal_mutex_lock(lp_mutex);
    num = lp_set_info_map->num;
    for (i = 0; i < num; i++) {
        user_lp_info_t* setptr = &lp_set_info_map->lp_modify_info_map[i];
        for (j = 0; j < TY_LP_MAX; j++) {
            if (setptr->type == ptr->type) {
                lp_info_p = ptr;
                memcpy((void*)lp_info_p, (void*)setptr, sizeof(user_lp_info_t));
                lpmgr_set_cpu_fix_sleep_time(setptr->mcu_lp_type);
                break;
            }
            ptr++;
        }
        if (lp_info_p) {
            PR_TRACE("Set lp_info_map  type:%d mcu_lp_type:%d dtim:%d timeout_range:%d\n",
                     lp_info_p->type, lp_info_p->mcu_lp_type, lp_info_p->dtim, lp_info_p->timeout_range / 1000);
        }
    }
    tal_mutex_unlock(lp_mutex);
    PR_TRACE("lpmgr_set_lp_info_map end\n");
}

int lpmgr_default_set(unsigned char max_dtim, user_set_map_t* lp_set_info_map)
{
    int ret = 0;

    if (NULL != lp_mutex) {
        PR_WARN("lpmgr already initialized, can't set");
        return ret;
    }

    if (max_dtim != 0) {
        defmin_dtim = max_dtim;
    }

    PR_TRACE("max dtim: %d", defmin_dtim);

    if (lp_set_info_map != NULL) {
        lpmgr_set_lp_info_map(lp_set_info_map);
    }
    return ret;
}

int lpmgr_updata_map_info(TY_LP_TYPE type, uint32_t sleep_time_s, uint32_t* cpu_sleep_time_s)
{
    PR_TRACE("type:%d, sleep_time_s:%d", type, sleep_time_s);
    uint32_t sleep_time_ms = 0;
    user_lp_info_t user_lp_info = {type, sleep_time_s * 1000, defmin_dtim, MAX_TIMEOUT};
    user_set_map_t user_set_map = {1, &user_lp_info};

    lpmgr_set_lp_info_map(&user_set_map);
    lpmgr_get_cpu_max_sleep_time(&sleep_time_ms);
    if (sleep_time_ms > 1000 && NULL != cpu_sleep_time_s) {
        *cpu_sleep_time_s = sleep_time_ms / 1000;
    } else {
        if (cpu_sleep_time_s) {
            *cpu_sleep_time_s = sleep_time_s / 1000;
        }
    }

    return OPRT_OK;
}

int lpmgr_init(void)
{
    int ret = 0;
    lp_info_t* lp_info_p = g_lp_info_map;

    if (NULL != lp_mutex) {
        PR_WARN("lpmgr already initialized.");
        goto exit;
    }

    mgr_init();

    ret = tal_mutex_create_init(&lp_mutex);
    if (0 != ret) {
        PR_ERR("Failed in tal_mutex_create_init ret:%d", ret);
        goto exit;
    }

    for (int i = 0; i < TY_LP_MAX; i++) {
        lp_info_p->cnt = 0;
        lp_info_p++;
    }

    if (defmin_dtim == TY_LP_DITM_30) {
        lp_info_current.mcu_lp_type = OS_LP_SLEEP_3S;
    } else if (defmin_dtim == TY_LP_DITM_20) {
        lp_info_current.mcu_lp_type = OS_LP_SLEEP_2S;
    } else {
        lp_info_current.mcu_lp_type = OS_LP_SLEEP_1S;
    }
    lp_info_current.dtim = defmin_dtim;

    tal_cpu_set_lp_mode(TRUE);
    lpmgr_enforce_mode(lp_info_current.mcu_lp_type, lp_info_current.dtim);
    tal_mutex_lock(lp_mutex);
    enforce = 0;
    tal_mutex_unlock(lp_mutex);

    tal_wifi_set_lps_dtim(defmin_dtim);
    PR_TRACE("lpmgr_init exit");
exit:
    return ret;
}

void lpmgr_show_power_mode(void)
{
    int i = 0;
    lp_info_t* lp_info_p = g_lp_info_map;

    tal_mutex_lock(lp_mutex);
    PR_INFO("%s Show lp_info_map:", __func__);
    for (i = 0; i < TY_LP_MAX; i++) {
        PR_INFO("%s type:%d mcu_lp_type:%d dtim:%d max_cnt:%d timeout_point:%04x cnt:%d", __func__,
                lp_info_p->type, lp_info_p->mcu_lp_type, lp_info_p->dtim, lp_info_p->max_cnt,
                (unsigned int)lp_info_p->timeout_point, lp_info_p->cnt);
        lp_info_p++;
    }

    PR_TRACE("%s Show current lp_info:", __func__);
    PR_INFO("%s type:%d mcu_lp_type:%d dtim:%d", __func__,
            lp_info_current.type, lp_info_current.mcu_lp_type, lp_info_current.dtim);
    tal_mutex_unlock(lp_mutex);

    return;
}

int lpmgr_enforce_mode(SYS_LP_TYPE mcu_lp_type, unsigned char dtim)
{
    int ret = 0;

    tal_mutex_lock(lp_mutex);
    if (mcu_lp_type == OS_LP_CLOSE) {
        ret = lpmgr_set_cpu_lp(false);
    } else {
        lpmgr_set_cpu_fix_sleep_time(mcu_lp_type);
        ret = lpmgr_set_cpu_lp(true);
    }

    if (ret != 0) {
        goto exit;
    }

    if (dtim != 0) {
        lpmgr_set_wifi_lp(false);
        tal_wifi_set_lps_dtim(dtim);
        lpmgr_set_wifi_lp(true);
    } else {
        lpmgr_set_wifi_lp(false);
    }

    enforce = 1;
exit:
    tal_mutex_unlock(lp_mutex);
    return ret;
}

void lpmgr_restore_mode(void)
{
    lpmgr_enforce_mode(lp_info_current.mcu_lp_type, lp_info_current.dtim);
    tal_mutex_lock(lp_mutex);
    enforce = 0;
    tal_mutex_unlock(lp_mutex);
}

void lpmgr_iot_clear(void)
{
    lpmgr_unregister(TY_LP_DNS);
    lpmgr_unregister(TY_LP_TCP_CONN);
    lpmgr_unregister(TY_LP_OTA);
    lpmgr_unregister(TY_LP_TLSCONN);
}

int lpmgr_set_cpu_fix_sleep_time(uint32_t time_ms)
{
    tal_mutex_lock(lp_mutex);
    g_cpu_sleep_time_ms = time_ms;
    tal_mutex_unlock(lp_mutex);
    return OPRT_OK;
}

int lpmgr_get_cpu_max_sleep_time(uint32_t* cpu_sleep_time_ms)
{
    *cpu_sleep_time_ms = TUYA_SVC_TIMER_TASK_CHECK_MIN;
    return OPRT_OK;
}

int lpmgr_set_lps_dtim(uint32_t dtim)
{
    if (TY_LP_DITM_30 != dtim && TY_LP_DITM_20 != dtim && TY_LP_DITM_10 != dtim) {
        return -1;
    }

    defmin_dtim = dtim;
    if (defmin_dtim == TY_LP_DITM_30) {
        g_lp_info_map = lp_dtim30_info_map;
    } else if (defmin_dtim == TY_LP_DITM_20) {
        g_lp_info_map = lp_dtim20_info_map;
    } else {
        g_lp_info_map = lp_info_map;
    }

    return 0;
}

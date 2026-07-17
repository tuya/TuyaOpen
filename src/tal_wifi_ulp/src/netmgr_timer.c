/**
 * @file netmgr_timer.c
 * @brief net manager timer - timeout management for ULP
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
#include "netmgr_timer.h"
#include "tuya_iot_config.h"
#include "tal_log.h"
#include "tal_time_service.h"
#include <stdio.h>

/***********************************************************
*************************variable define********************
***********************************************************/
struct netmgr_timer ntimer;
struct netmgr_timer lptimer;

/***********************************************************
*************************function define********************
***********************************************************/
uint64_t current_timestamp(void)
{
    TIME_S sectime = 0;
    TIME_MS mstime = 0;
    uint64_t cur_time_ms = 0;

    tal_time_get_system_time(&sectime, &mstime);
    cur_time_ms = (uint64_t)sectime * 1000 + mstime;

    return cur_time_ms;
}

void netmgr_set_timer(struct netmgr_timer* timer, int32_t time_ms, TIMER_HANDLER handler)
{
    if (NULL == timer || NULL == handler || 0 >= time_ms) {
        return;
    }

    timer->handler = handler;
    timer->time_ms = time_ms + current_timestamp();
    return;
}

void netmgr_clear_timer(struct netmgr_timer* timer)
{
    if (NULL == timer) {
        PR_ERR("invalid param timer:%p", timer);
        return;
    }

    timer->handler = NULL;
    timer->time_ms = 0;

    return;
}

void netmgr_timer_run(void)
{
    TIMER_HANDLER handler = NULL;

    if (NULL != ntimer.handler && 0 != ntimer.time_ms) {
        if (ntimer.time_ms <= current_timestamp()) {
            handler = ntimer.handler;
            ntimer.handler = NULL;
            ntimer.time_ms = 0;
            handler(&ntimer);
        }
    }

    if (NULL != lptimer.handler && 0 != lptimer.time_ms) {
        if (lptimer.time_ms <= current_timestamp()) {
            handler = lptimer.handler;
            lptimer.handler = NULL;
            lptimer.time_ms = 0;
            handler(&lptimer);
        }
    }

    return;
}

int netmgr_timer_registered(struct netmgr_timer* timer)
{
    if (NULL == timer) {
        PR_ERR("Invalid args timer:%p \n", timer);
        return 0;
    }

    if (0 == timer->time_ms || NULL == timer->handler) {
        return 0;
    } else {
        return 1;
    }
}

unsigned int netmgr_get_timer_sleeptime(void)
{
    int nsleeptime = 0;
    int lpsleeptime = 0;

    if (0 < ntimer.time_ms) {
        nsleeptime = ntimer.time_ms - current_timestamp();
    }

    if (0 < lptimer.time_ms) {
        lpsleeptime = lptimer.time_ms - current_timestamp();
    }

    if (0 < nsleeptime && 0 < lpsleeptime) {
        return ((lpsleeptime < nsleeptime) ? lpsleeptime : nsleeptime);
    } else if (0 < nsleeptime) {
        return nsleeptime;
    } else if (0 < lpsleeptime) {
        return lpsleeptime;
    } else {
        return 0;
    }
}

/**
 * @file netmgr.c
 * @brief net manager - message queue driven API scheduling thread for ULP
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
#include "netmgr.h"
#include "netmgr_timer.h"
#include "tal_queue.h"
#include "tuya_iot_config.h"
#include "tal_log.h"
#include "tal_thread.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tal_memory.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define QUEUE_WAIT_FOREVER 0xFFFFFFFF
#define QUEUE_WAIT_100MS 100
#define NETMGR_QUEUE_SIZE 32
#define NETMGR_THREAD_STACK_SIZE (2048)
#define NETMGR_GHREAD_PRIORITY 2
#define NETMNG_MSG_SIZE sizeof(netmgr_msg)

/***********************************************************
*************************variable define********************
***********************************************************/
static QUEUE_HANDLE netmgr_queue = NULL;
static THREAD_HANDLE pnetmgr_thread;
static int timer_need_refresh = 1;

static int netmgr_timeouts_queue_fetch(QUEUE_HANDLE queue, void** msg)
{
    unsigned int sleeptime;
    int ret = 0;

    sleeptime = netmgr_get_timer_sleeptime();
    if (0 == sleeptime) {
        sleeptime = QUEUE_WAIT_FOREVER;
    }

    ret = tal_queue_fetch(queue, msg, sleeptime);

    return ret;
}

static void netmgr_handle_msg(netmgr_msg* msg)
{
    switch (msg->type) {
    case NETMGR_API_TYPE:
        if (msg->msg.api_msg.function) {
            msg->msg.api_msg.function(msg);
        }
        break;

    case NETMGR_NTFY_TYPE:
        break;

    default:
        break;
    }
}

static void netmgr_thread(void* param)
{
    netmgr_msg* msg;
    int ret = 0;

    if (NULL == netmgr_queue) {
        return;
    }
    PR_DEBUG("netmgr_thread run");
    while (1) {
        ret = netmgr_timeouts_queue_fetch(netmgr_queue, (void**)&msg);
        if (NULL != msg && 0 == ret) {
            netmgr_handle_msg(msg);
        }

        netmgr_timer_run();
    }
}

int netmgr_api_call(netmgr_msg* msg)
{
    return tal_queue_post(netmgr_queue, (void*)&msg, QUEUE_WAIT_FOREVER);
}

static int netmgr_api_call_with_ret(netmgr_api_callback_fn a, void* b)
{
    netmgr_msg* c = tal_malloc(NETMNG_MSG_SIZE);
    if (NULL == c) {
        return -1;
    }
    c->type = NETMGR_API_TYPE;
    c->msg.api_msg.arg = b;
    c->msg.api_msg.function = a;
    return netmgr_api_call(c);
}

static void void_func(void* arg)
{
    PR_DEBUG("queue post trigger timer refresh");
    timer_need_refresh = 1;
    tal_free(arg);

    return;
}

int netmgr_timer_refresh(void)
{
    PR_DEBUG("timer_need_refresh: %d", timer_need_refresh);
    if (timer_need_refresh) {
        timer_need_refresh = 0;
        int ret = netmgr_api_call_with_ret(void_func, NULL);
        if (0 != ret) {
            timer_need_refresh = 1;
        }
    }

    return 0;
}

int mgr_init(void)
{
    if (netmgr_queue) {
        return -1;
    }

    int ret = tal_queue_create_init(&netmgr_queue, sizeof(void*), NETMGR_QUEUE_SIZE);
    if (0 != ret) {
        return ret;
    }

    THREAD_CFG_T thread_cfg = {
        .stackDepth = NETMGR_THREAD_STACK_SIZE,
        .priority = NETMGR_GHREAD_PRIORITY,
        .thrdname = "ulp_timer"
    };
    ret = tal_thread_create_and_start(&pnetmgr_thread, NULL, NULL, netmgr_thread, NULL, &thread_cfg);
    if (0 != ret) {
        tal_queue_free(netmgr_queue);
        netmgr_queue = NULL;
        return ret;
    }

    return 0;
}

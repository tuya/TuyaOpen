/**
 * @file dev_evt.c
 * @brief Device network-operation event notification dispatch.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "dev_evt.h"

static DEV_EVT_CB s_dev_evt_cb = NULL;

void tuya_dev_evt_set_cb(DEV_EVT_CB cb)
{
    s_dev_evt_cb = cb;
}

void tuya_dev_evt_notify(DEV_EVT_E evt, DEV_ACTION_E action, void *ctx)
{
    DEV_EVT_CB cb = s_dev_evt_cb;
    if (NULL != cb) {
        cb(evt, action, ctx);
    }
}

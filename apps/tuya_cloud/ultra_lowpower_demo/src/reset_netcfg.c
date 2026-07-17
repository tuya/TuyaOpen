/**
 * @file reset_netcfg.c
 * @brief Implements reset network configuration functionality for IoT devices
 *
 * This source file provides the implementation of the reset network configuration
 * functionality required for IoT devices. It includes functionality for managing
 * reset counters, handling reset events, and clearing network configurations.
 * The implementation supports integration with the Tuya IoT platform and ensures
 * proper handling of reset-related operations. This file is essential for developers
 * working on IoT applications that require robust network configuration reset mechanisms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tuya_iot.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define RESET_NETCNT_NAME "rst_cnt"
#define RESET_NETCNT_MAX  3

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

static int reset_count_read(uint8_t *count)
{
    int rt = OPRT_OK;

    uint8_t *read_buf = NULL;
    size_t read_len;

    TUYA_CALL_ERR_RETURN(tal_kv_get(RESET_NETCNT_NAME, &read_buf, &read_len));
    *count = read_buf[0];

    PR_DEBUG("reset count is %d", *count);

    if (NULL != read_buf) {
        tal_kv_free(read_buf);
        read_buf = NULL;
    }

    return rt;
}

static int reset_count_write(uint8_t count)
{
    PR_DEBUG("reset count write %d", count);
    return tal_kv_set(RESET_NETCNT_NAME, &count, 1);
}

static void reset_netconfig_timer(TIMER_ID timer_id, void *arg)
{
    reset_count_write(0);
    PR_DEBUG("reset cnt clear!");
}

static OPERATE_RET __reset_netconfig_clear(void *data)
{
    reset_count_write(0);
    PR_DEBUG("reset cnt clear by reset event!");
    return OPRT_OK;
}

int reset_netconfig_check(void)
{
    int rt;
    uint8_t rst_cnt = 0;

    TUYA_CALL_ERR_LOG(reset_count_read(&rst_cnt));
    if (rst_cnt < RESET_NETCNT_MAX) {
        return OPRT_OK;
    }

    tal_event_subscribe(EVENT_RESET, "reset_netconfig", __reset_netconfig_clear, SUBSCRIBE_TYPE_NORMAL);

    PR_DEBUG("Reset ctrl data!");
    tuya_iot_reset(tuya_iot_client_get());

    return rt;
}

int reset_netconfig_start(void)
{
    int rt = OPRT_OK;
    uint8_t rst_cnt = 0;

    TUYA_CALL_ERR_LOG(reset_count_read(&rst_cnt));
    TUYA_CALL_ERR_LOG(reset_count_write(++rst_cnt));

    PR_DEBUG("start reset cnt clear timer!!!!!");
    TIMER_ID rst_config_timer;
    tal_sw_timer_create(reset_netconfig_timer, NULL, &rst_config_timer);
    tal_sw_timer_start(rst_config_timer, 5000, TAL_TIMER_ONCE);

    return OPRT_OK;
}

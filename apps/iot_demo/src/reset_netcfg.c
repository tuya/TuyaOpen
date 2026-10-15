/**
 * @file reset_netcfg.c
 * @brief 连续复位清配网：复位计数 + 复位事件处理 + 清配网
 *
 * 从上游 ipc_p2p_demo 的 src/reset_netcfg.c 逐行移植，差异仅两处：
 *   1. `tal_kv_free(read_buf)` → `tal_free(read_buf)`（本仓库 tal_kv 只有
 *      init/set/get/del 四接口，get 的返回值约定用 tal_free 释放，见 tal_kv.h）；
 *   2. `reset_netconfig_check()` 的返回值局部变量补初值（上游 `int rt;` 仅在
 *      `reset_count_read` 成功时经 TUYA_CALL_ERR_LOG 赋值后 `return rt;`，
 *      失败路径提前返回——是"脆弱但良定义"，不是 UB；补初值属防御性初始化，
 *      语义不变）。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
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
    size_t   read_len;

    TUYA_CALL_ERR_RETURN(tal_kv_get(RESET_NETCNT_NAME, &read_buf, &read_len));
    *count = read_buf[0];

    PR_DEBUG("reset count is %d", *count);

    if (NULL != read_buf) {
        tal_free(read_buf);
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

/**
 * @brief Checks the status of the network configuration reset process.
 *
 * This function verifies whether the network configuration reset process
 * has been completed successfully or is still in progress.
 *
 * @return int Returns 0 on success, or a negative value on failure.
 */
int reset_netconfig_check(void)
{
    int     rt      = OPRT_OK;
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

/**
 * @brief Starts the network configuration reset process.
 *
 * This function initiates the process to reset the network configuration
 * of the device. It is typically used to clear existing network settings
 * and prepare the device for reconfiguration.
 *
 * @return int Returns 0 on success, or a negative value on failure.
 */
int reset_netconfig_start(void)
{
    int     rt      = OPRT_OK;
    uint8_t rst_cnt = 0;

    TUYA_CALL_ERR_LOG(reset_count_read(&rst_cnt));
    TUYA_CALL_ERR_LOG(reset_count_write(++rst_cnt));

    PR_DEBUG("start reset cnt clear timer!!!!!");
    TIMER_ID rst_config_timer;
    tal_sw_timer_create(reset_netconfig_timer, NULL, &rst_config_timer);
    tal_sw_timer_start(rst_config_timer, 5000, TAL_TIMER_ONCE);

    return OPRT_OK;
}

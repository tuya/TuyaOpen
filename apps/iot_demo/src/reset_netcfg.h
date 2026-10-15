/**
 * @file reset_netcfg.h
 * @brief 连续复位清配网功能：对外声明
 *
 * 从上游 ipc_p2p_demo 的 include/reset_netcfg.h 移植（保留原注释风格）。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __RESET_NETCFG_H__
#define __RESET_NETCFG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief 启动配网复位流程：复位计数 +1，并起 5 秒单次定时器清零计数。
 *
 * 正常启动（5 秒内不再复位）计数会被清零；连续复位 3 次则在下一次
 * reset_netconfig_check() 时清掉配网数据。
 *
 * @return int 成功返回 0，失败返回负值。
 */
int reset_netconfig_start(void);

/**
 * @brief 检查复位计数是否达到阈值，达到则清配网。
 *
 * 计数 >= 3 时订阅 EVENT_RESET（复位事件后清零计数）并调用
 * tuya_iot_reset() 清除配网数据。
 *
 * @return int 成功返回 0，失败返回负值。
 */
int reset_netconfig_check(void);

#ifdef __cplusplus
}
#endif

#endif /* __RESET_NETCFG_H__ */

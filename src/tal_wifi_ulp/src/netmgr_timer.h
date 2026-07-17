/**
 * @file netmgr_timer.h
 * @brief Timer management for ULP netmgr
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
#ifndef __ULP_NETMGR_TIMER_H__
#define __ULP_NETMGR_TIMER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct netmgr_timer;
typedef void (*TIMER_HANDLER)(struct netmgr_timer* timer);

struct netmgr_timer {
    TIMER_HANDLER handler;
    uint64_t time_ms;
};

/**
 * @brief set netmgr timer
 *
 * @param[in]       time_ms         Time required for timeout
 * @param[in]       handler         callback after timeout
 * @return          no return
 */
void netmgr_set_timer(struct netmgr_timer* timer, int32_t time_ms, TIMER_HANDLER handler);

/**
 * @brief clear netmgr timer
 *
 * @param[in]       no args
 * @return          no return
 */
void netmgr_clear_timer(struct netmgr_timer* timer);

/**
 * @brief check if timeout occured. when timeout occured call callback function.
 *
 * @param[in]       no args
 * @return          no return
 */
void netmgr_timer_run(void);

/**
 * @brief check if timer registered.
 *
 * @param[in]       no args
 * @return      1: timer already set.
 *              0: timer not set.
 */
int netmgr_timer_registered(struct netmgr_timer* timer);

/**
 * @brief get timer sleeptime.
 *
 * @param[in]       no args
 * @return          sleeptime.
 */
unsigned int netmgr_get_timer_sleeptime(void);

/**
 * @brief get current timestamp.
 *
 * @param[in]       no args
 * @return          current timestamp.
 */
uint64_t current_timestamp(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ULP_NETMGR_TIMER_H__ */

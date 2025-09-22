/**
 * @file tal_sleep.c
 * @brief Implements low power and sleep mode functionalities for Tuya IoT
 * applications.
 *
 * This source file provides the implementation for managing low power and sleep
 * modes in Tuya IoT applications. It encapsulates the functionality to set the
 * CPU sleep mode and manage low power states to optimize power consumption. The
 * API abstracts underlying hardware-specific sleep mechanisms, offering a
 * unified interface for application-level power management.
 *
 * Key functionalities include:
 * - Setting the CPU sleep mode to control power consumption.
 * - Managing low power states through reference counting to prevent unintended
 * wake-ups.
 * - Thread-safe operations for setting low power modes.
 *
 * The implementation utilizes a static structure to maintain the state of low
 * power mode management, including enabling/disabling low power modes, counting
 * the number of low power mode settings, and a mutex for thread-safe
 * operations.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_sleep.h"
#include "tal_mutex.h"
#include "tal_log.h"
#include "tkl_sleep.h"

typedef struct {
    BOOL_T lp_enable;
    uint8_t lp_mode_cnt;
    MUTEX_HANDLE lp_mutex;
    uint32_t lp_disable_cnt;
} TAL_CPU_T;

static TAL_CPU_T s_tal_cpu = {0};

/**
 * @brief Sets the CPU sleep mode.
 *
 * This function sets the CPU sleep mode to the specified mode.
 *
 * @param enable Whether to enable or disable the CPU sleep mode.
 * @param mode The CPU sleep mode to set.
 *
 * @return The result of the operation.
 */
OPERATE_RET tal_cpu_sleep_mode_set(BOOL_T enable, TUYA_CPU_SLEEP_MODE_E mode)
{
    return tkl_cpu_sleep_mode_set(enable, mode);
}

/**
 * Sets the low power mode for the CPU.
 *
 * @param lp_enable Boolean value indicating whether to enable low power mode.
 */
void tal_cpu_set_lp_mode(BOOL_T lp_enable)
{
    // function can be called only once
    PR_DEBUG("cpu_set_lp_mode_cnt:%d", s_tal_cpu.lp_mode_cnt);
    if (s_tal_cpu.lp_mode_cnt > 0) {
        return;
    }
    if (lp_enable) {
        tal_cpu_sleep_mode_set(TRUE, TUYA_CPU_SLEEP);
    }
    PR_DEBUG("set cpu lp mode:%d", lp_enable);
    s_tal_cpu.lp_enable = lp_enable;
    s_tal_cpu.lp_mode_cnt++;
}

/**
 * @brief Retrieves the low power mode status of the CPU.
 *
 * This function returns the status of the low power mode of the CPU.
 *
 * @return The low power mode status of the CPU.
 */
BOOL_T tal_cpu_get_lp_mode(void)
{
    return s_tal_cpu.lp_enable;
}

static OPERATE_RET tal_cpu_lp_init_mutex(void)
{
    if (NULL != s_tal_cpu.lp_mutex) {
        return OPRT_OK;
    }

    OPERATE_RET op_ret = tal_mutex_create_init(&s_tal_cpu.lp_mutex);
    if (OPRT_OK != op_ret) {
        PR_ERR("create mutex fail");
    }

    return op_ret;
}

/**
 * @brief Enable low power mode for the CPU.
 *
 * This function enables low power mode for the CPU. It checks if low power mode
 * is enabled, initializes the necessary mutex, and then locks the mutex to
 * ensure exclusive access. If the low power disable count is greater than 0, it
 * decrements the count. Finally, if the low power disable count is 0, it sets
 * the CPU sleep mode to TRUE.
 *
 * @return The result of the operation. OPRT_OK if successful, an error code
 * otherwise.
 */
OPERATE_RET tal_cpu_lp_enable(void)
{
    OPERATE_RET op_ret = OPRT_OK;
    if (!tal_cpu_get_lp_mode()) {
        PR_DEBUG("can not enable, lowpower disabled");
        return OPRT_COM_ERROR;
    }
    op_ret = tal_cpu_lp_init_mutex();
    if (OPRT_OK != op_ret) {
        return op_ret;
    }
    tal_mutex_lock(s_tal_cpu.lp_mutex);
    if (s_tal_cpu.lp_disable_cnt > 0) {
        s_tal_cpu.lp_disable_cnt--;
    }
    PR_DEBUG("<tal_cpu_lp> disable_cnt:%d", s_tal_cpu.lp_disable_cnt);
    if (!s_tal_cpu.lp_disable_cnt) {
        op_ret = tal_cpu_sleep_mode_set(TRUE, TUYA_CPU_SLEEP);
    }
    tal_mutex_unlock(s_tal_cpu.lp_mutex);
    if (OPRT_OK != op_ret) {
        PR_ERR("cpu_lp_enable: set cpu lp mode fail(%d)", op_ret);
    }

    return op_ret;
}

/**
 * @brief Disable low power mode for the CPU.
 *
 * This function disables the low power mode for the CPU. If the CPU is already
 * disabled, it returns OPRT_OK without making any changes.
 *
 * @return The result of the operation. OPRT_OK if the CPU is successfully
 * disabled, an error code otherwise.
 */
OPERATE_RET tal_cpu_lp_disable(void)
{
    OPERATE_RET op_ret = OPRT_OK;

    if (!tal_cpu_get_lp_mode()) {
        PR_DEBUG("cpu has been disabled");
        return OPRT_OK;
    }

    op_ret = tal_cpu_lp_init_mutex();
    if (OPRT_OK != op_ret) {
        return op_ret;
    }

    tal_mutex_lock(s_tal_cpu.lp_mutex);

    if (!s_tal_cpu.lp_disable_cnt++) {
        op_ret = tal_cpu_sleep_mode_set(FALSE, TUYA_CPU_SLEEP);
    }

    PR_DEBUG("<tal_cpu_lp>  disable_cnt:%d", s_tal_cpu.lp_disable_cnt);

    tal_mutex_unlock(s_tal_cpu.lp_mutex);

    if (OPRT_OK != op_ret) {
        PR_ERR("tuya_cpu_lp_disable: set cpu lp mode fail(%d)", op_ret);
    }

    return op_ret;
}

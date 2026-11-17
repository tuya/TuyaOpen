/**
 * @file tuya_t9_ex_module.h
 * @brief Board-level module configuration and registration API for the T9 EVB.
 *
 * Display is the first module on this board. Camera and touch registration will
 * be added here as they are brought up.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_T9_EX_MODULE_H__
#define __TUYA_T9_EX_MODULE_H__

#include "tuya_cloud_types.h"

#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
#include "tdd_disp_jd9165t.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if defined(ENABLE_DISPLAY) && (ENABLE_DISPLAY == 1)
/* TUYA_GPIO_NUM_MAX means "not wired" and the step is skipped. */
#define BOARD_LCD_RESET_PIN             TUYA_GPIO_NUM_7
#define BOARD_LCD_RESET_ACTIVE_LEVEL    TUYA_GPIO_LEVEL_LOW
#define BOARD_LCD_BL_PIN                TUYA_GPIO_NUM_5
#define BOARD_LCD_BL_ACTIVE_LEVEL       TUYA_GPIO_LEVEL_HIGH
#define BOARD_LCD_POWER_PIN             TUYA_GPIO_NUM_MAX
#endif

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers the optional board modules (display today, camera and touch later).
 *
 * @return Returns OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET board_register_ex_module(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_T9_EX_MODULE_H__ */

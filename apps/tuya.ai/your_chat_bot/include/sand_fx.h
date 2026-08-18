/**
 * @file sand_fx.h
 * @brief Gravity sand / water demo driven by SH3001
 * @version 0.3
 * @date 2026-08-11
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __SAND_FX_H__
#define __SAND_FX_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create sand screen (timer paused, screen not loaded)
 * @return OPRT_OK on success
 * @note Call after LVGL is ready. Does not take over the display.
 */
OPERATE_RET sand_fx_init(void);

/**
 * @brief Show sand screen and resume physics
 * @return OPRT_OK on success
 */
OPERATE_RET sand_fx_enter(void);

/**
 * @brief Pause physics (keep screen object)
 * @return none
 */
void sand_fx_leave(void);

/**
 * @brief Get sand LVGL screen object
 * @return screen pointer, or NULL if not inited
 */
void *sand_fx_get_screen(void);

/**
 * @brief Legacy auto-start (creates and enters after delay) — prefer launcher
 * @return OPRT_OK on success
 */
OPERATE_RET sand_fx_start(void);

#ifdef __cplusplus
}
#endif

#endif /* __SAND_FX_H__ */

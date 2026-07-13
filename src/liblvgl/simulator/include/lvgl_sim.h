/**
 * @file lvgl_sim.h
 * @brief LVGL PC simulator interface
 * @version 1.0
 * @date 2025-04-30
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __LVGL_SIM_H__
#define __LVGL_SIM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI entry callback type
 */
typedef void (*LVGL_SIM_ENTRY_CB)(void);

/**
 * @brief Start LVGL simulator with SDL2
 *
 * Init LVGL, create SDL window/mouse/keyboard, call entry_cb,
 * then run lv_timer_handler loop. Does not return.
 *
 * @param[in] entry_cb UI init function (e.g. ui_init)
 * @return none
 */
void lvgl_sim_start(LVGL_SIM_ENTRY_CB entry_cb);

#ifdef __cplusplus
}
#endif
#endif /* __LVGL_SIM_H__ */

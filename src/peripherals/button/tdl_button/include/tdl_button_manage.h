/**
 * @file tdl_button_manage.h
 * @brief Tuya Driver Layer button management interface.
 *
 * This file provides the high-level button management interface for the Tuya
 * Driver Layer (TDL). It defines structures and functions for creating, configuring,
 * and managing button devices with advanced event detection capabilities. The
 * interface supports various button events including single click, double click,
 * multiple clicks, and long press with configurable timing parameters.
 *
 * Key features:
 * - Multiple button event types (press, release, clicks, long press)
 * - Configurable debouncing and timing parameters
 * - Event callback mechanism for button state changes
 * - Button lifecycle management (create, delete, enable, disable)
 * - Support for both interrupt and polling-based implementations
 *
 * The TDL button management layer provides sophisticated button event detection
 * built on top of the basic button driver interface, enabling rich user
 * interaction patterns in Tuya IoT devices.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _TDL_BUTTON_MANAGE_H_
#define _TDL_BUTTON_MANAGE_H_

#include "tuya_cloud_types.h"
#include "tdl_button_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef void *TDL_BUTTON_HANDLE;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    TDL_BUTTON_PRESS_DOWN = 0,     // Press down trigger
    TDL_BUTTON_PRESS_UP,           // Release trigger
    TDL_BUTTON_PRESS_SINGLE_CLICK, // Single click trigger
    TDL_BUTTON_PRESS_DOUBLE_CLICK, // Double click trigger
    TDL_BUTTON_PRESS_REPEAT,       // Multiple click trigger
    TDL_BUTTON_LONG_PRESS_START,   // Long press start trigger
    TDL_BUTTON_LONG_PRESS_HOLD,    // Long press hold trigger
    TDL_BUTTON_RECOVER_PRESS_UP,   // Triggered when the effective level is maintained after power-on and then restored
    TDL_BUTTON_PRESS_MAX,          // None
    TDL_BUTTON_PRESS_NONE,         // None
} TDL_BUTTON_TOUCH_EVENT_E;        // Button trigger event

typedef struct {
    uint16_t long_start_valid_time; // Long press start valid time (ms): e.g., 3000 - triggers after 3s long press
    uint16_t long_keep_timer; // Long press hold trigger time (ms): e.g., 100ms - triggers every 100ms during long press
    uint16_t button_debounce_time; // Debounce time (ms)
    uint8_t
        button_repeat_valid_count; // Number of multiple clicks to trigger, triggers multi-click event if greater than 2
    uint16_t button_repeat_valid_time; // Valid interval for double/multiple clicks (ms), double-click is invalid if 0
} TDL_BUTTON_CFG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
/**
 * @brief button event callback function
 * @param[in] name button name
 * @param[in] event button trigger event
 * @param[in] argc repeat count/long press time
 * @return none
 */
typedef void (*TDL_BUTTON_EVENT_CB)(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Pass in the button configuration and create a button handle
 * @param[in] name button name
 * @param[in] button_cfg button software configuration
 * @param[out] handle the handle of the control button
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_button_create(char *name, TDL_BUTTON_CFG_T *button_cfg, TDL_BUTTON_HANDLE *handle);

/**
 * @brief Delete a button
 * @param[in] handle the handle of the control button
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_button_delete(TDL_BUTTON_HANDLE handle);

/**
 * @brief Delete a button without tdd info
 * @param[in] handle the handle of the control button
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_button_delete_without_hardware(TDL_BUTTON_HANDLE handle);

/**
 * @brief Function registration for button events
 * @param[in] handle the handle of the control button
 * @param[in] event button trigger event
 * @param[in] cb The function corresponding to the button event
 * @return none
 */
void tdl_button_event_register(TDL_BUTTON_HANDLE handle, TDL_BUTTON_TOUCH_EVENT_E event, TDL_BUTTON_EVENT_CB cb);

/**
 * @brief Turn button function off or on
 * @param[in] enable 0-close  1-open
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_button_deep_sleep_ctrl(uint8_t enable);

/**
 * @brief set button task stack size
 *
 * @param[in] size stack size
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_button_set_task_stack_size(uint32_t size);

/**
 * @brief set button ready flag (sensor special use)
 *		 if ready flag is false, software will filter the trigger for the first time,
 *		 if use this func,please call after registered.
 *        [ready flag default value is false.]
 * @param[in] name button name
 * @param[in] status true or false
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_button_set_ready_flag(char *name, uint8_t status);

/**
 * @brief read button status
 * @param[in] handle button handle
 * @param[out] status button status
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_button_read_status(TDL_BUTTON_HANDLE handle, uint8_t *status);

/**
 * @brief set button level ( rocker button use)
 *		 The default configuration is toggle switch - when level flipping,
 *		 it is modified to level synchronization in the application - the default effective level is low effective
 * @param[in] handle button handle
 * @param[in] level TUYA_GPIO_LEVEL_E
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_button_set_level(TDL_BUTTON_HANDLE handle, TUYA_GPIO_LEVEL_E level);

/**
 * @brief set button scan time, default is 10ms
 * @param[in] time_ms button scan time
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_button_set_scan_time(uint8_t time_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDL_BUTTON_MANAGE_H_*/
/**
 * @file tdl_button_manage.h
 * @author franky.lin@tuya.com
 * @brief tdl_button_manage, base timer、semaphore、task
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * button trigger management component
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
    TDL_BUTTON_PRESS_DOWN = 0,     // 按下触发
    TDL_BUTTON_PRESS_UP,           // 松开触发
    TDL_BUTTON_PRESS_SINGLE_CLICK, // 单击触发
    TDL_BUTTON_PRESS_DOUBLE_CLICK, // 双击触发
    TDL_BUTTON_PRESS_REPEAT,       // 多击触发
    TDL_BUTTON_LONG_PRESS_START,   // 长按开始触发
    TDL_BUTTON_LONG_PRESS_HOLD,    // 长按保持触发
    TDL_BUTTON_RECOVER_PRESS_UP,   // 上电如果一直保持有效电平恢复后触发
    TDL_BUTTON_PRESS_MAX,          // 无
    TDL_BUTTON_PRESS_NONE,         // 无
} TDL_BUTTON_TOUCH_EVENT_E;        // 按键触发事件

typedef struct {
    uint16_t long_start_valid_time;    // 按键长按开始有效时间(ms)：e.g  3000-长按3s触发
    uint16_t long_keep_timer;          // 按键长按持续触发时间(ms)：e.g 100ms-长按时每100ms触发一次
    uint16_t button_debounce_time;     // 消抖时间(ms)
    uint8_t button_repeat_valid_count; // 多击触发次数,大于2触发多击事件
    uint16_t button_repeat_valid_time; // 双击、多击触发有效间隔时间(ms)，为0双击事件无效
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
/**
 * @file tdl_joystick_manage.h
 * @author franky.lin@tuya.com
 * @brief tdl_joystick_manage, base timer、semaphore、task
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * joystick trigger management component
 */

#ifndef _TDL_JOYSTICK_MANAGE_H_
#define _TDL_JOYSTICK_MANAGE_H_

#include "tuya_cloud_types.h"
#include "tdl_joystick_driver.h"
#include "tdl_button_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
typedef void *TDL_JOYSTICK_HANDLE;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    TDL_JOYSTICK_BUTTON_PRESS_DOWN = 0,     // 按下触发
    TDL_JOYSTICK_BUTTON_PRESS_UP,           // 松开触发
    TDL_JOYSTICK_BUTTON_PRESS_SINGLE_CLICK, // 单击触发
    TDL_JOYSTICK_BUTTON_PRESS_DOUBLE_CLICK, // 双击触发
    TDL_JOYSTICK_BUTTON_PRESS_REPEAT,       // 多击触发
    TDL_JOYSTICK_BUTTON_LONG_PRESS_START,   // 长按开始触发
    TDL_JOYSTICK_BUTTON_LONG_PRESS_HOLD,    // 长按保持触发
    TDL_JOYSTICK_BUTTON_RECOVER_PRESS_UP,   // 上电如果一直保持有效电平恢复后触发

    TDL_JOYSTICK_UP,
    TDL_JOYSTICK_DOWN,
    TDL_JOYSTICK_LEFT,
    TDL_JOYSTICK_RIGHT,
    TDL_JOYSTICK_LONG_UP,
    TDL_JOYSTICK_LONG_DOWN,
    TDL_JOYSTICK_LONG_LEFT,
    TDL_JOYSTICK_LONG_RIGHT,
    TDL_JOYSTICK_TOUCH_EVENT_MAX,
    TDL_JOYSTICK_TOUCH_EVENT_NONE,
} TDL_JOYSTICK_TOUCH_EVENT_E;        // 摇杆触发事件

typedef struct {
    uint16_t adc_max_val;       // adc max value
    uint16_t adc_min_val;       // adc min value
    uint16_t normalized_range;  // adc normalized range
    uint8_t sensitivity;        // <= normalized_range
    uint8_t channel_x;          // adc channel x
    uint8_t channel_y;          // adc channel y
} TDL_ADC_CFG_T;

typedef struct {
    TDL_BUTTON_CFG_T button_cfg;
    TDL_ADC_CFG_T adc_cfg;
} TDL_JOYSTICK_CFG_T;
/***********************************************************
***********************variable define**********************
***********************************************************/
/**
 * @brief joystick event callback function
 * @param[in] name joystick name
 * @param[in] event joystick trigger event
 * @param[in] argc repeat count/long press time
 * @return none
 */
typedef void (*TDL_JOYSTICK_EVENT_CB)(char *name, TDL_JOYSTICK_TOUCH_EVENT_E event, void *argc);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Pass in the button configuration and create a button handle
 * @param[in] name joystick name
 * @param[in] joystick_cfg joystick software configuration
 * @param[out] handle the handle of the control joystick
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_create(char *name, TDL_JOYSTICK_CFG_T *joystick_cfg, TDL_JOYSTICK_HANDLE *handle);

/**
 * @brief Delete a joystick
 * @param[in] handle the handle of the control joystick
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_delete(TDL_JOYSTICK_HANDLE handle);

/**
 * @brief Delete a joystick without tdd info
 * @param[in] handle the handle of the control joystick
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_delete_without_hardware(TDL_JOYSTICK_HANDLE handle);

/**
 * @brief Function registration for joystick events
 * @param[in] handle the handle of the control joystick
 * @param[in] event joystick trigger event
 * @param[in] cb The function corresponding to the joystick event
 * @return none
 */
void tdl_joystick_event_register(TDL_JOYSTICK_HANDLE handle, TDL_JOYSTICK_TOUCH_EVENT_E event, TDL_JOYSTICK_EVENT_CB cb);

/**
 * @brief Turn joystick function off or on
 * @param[in] enable 0-close  1-open
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_deep_sleep_ctrl(uint8_t enable);

/**
 * @brief set joystick task stack size
 *
 * @param[in] size stack size
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdl_joystick_set_task_stack_size(uint32_t size);

/**
 * @brief set joystick ready flag (sensor special use)
 *		 if ready flag is false, software will filter the trigger for the first time,
 *		 if use this func,please call after registered.
 *        [ready flag default value is false.]
 * @param[in] name button name
 * @param[in] status true or false
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_set_ready_flag(char *name, uint8_t status);

/**
 * @brief read joystick status
 * @param[in] handle button handle
 * @param[out] status button status
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_read_status(TDL_JOYSTICK_HANDLE handle, uint8_t *status);

/**
 * @brief set joystick level ( rocker button use)
 *		 The default configuration is toggle switch - when level flipping,
 *		 it is modified to level synchronization in the application - the default effective level is low effective
 * @param[in] handle joystick handle
 * @param[in] level TUYA_GPIO_LEVEL_E
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_set_level(TDL_JOYSTICK_HANDLE handle, TUYA_GPIO_LEVEL_E level);

/**
 * @brief set joystick scan time, default is 10ms
 * @param[in] time_ms joystick scan time
 * @return OPRT_OK if successful
 */
OPERATE_RET tdl_joystick_set_scan_time(uint8_t time_ms);

/** 
 * @brief get joystick raw data
 * @param[in] handle joystick handle
 * @param[in] channel_x x axis adc channel
 * @param[in] channel_y y axis adc channel
 * @param[out] x x axis data
 * @param[out] y y axis data
 */
void tdl_joystick_get_raw_xy(TDL_JOYSTICK_HANDLE handle, int channel_x, int channel_y, int *x, int *y);

/**
 * @brief get joystick normalized data
 * @param[in] handle joystick handle
 * @param[in] channel_x x axis adc channel
 * @param[in] channel_y y axis adc channel
 * @param[out] x x axis data
 * @param[out] y y axis data
 * 
 * @return OPRT_OK if successful
 */
void tdl_joystick_calibrated_xy(TDL_JOYSTICK_HANDLE handle, int channel_x, int channel_y, int *x, int *y);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDL_JOYSTICK_MANAGE_H_*/
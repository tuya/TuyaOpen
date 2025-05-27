/**
 * @file tdl_led_manage.h
 * @brief LED Management Interface Header File
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __TDL_LED_MANAGE_H__
#define __TDL_LED_MANAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TDL_BLINK_FOREVER             (0xFFFFFFFFu) 

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void* TDL_LED_HANDLE_T;

typedef enum {
    TDL_LED_MODE_OFF, 
    TDL_LED_MODE_ON,  
    TDL_LED_MODE_FLASH,
    TDL_LED_MODE_BLINK, 
} TDL_LED_MODE_E;

typedef enum {
    TDL_LED_OFF, 
    TDL_LED_ON,  
    TDL_LED_TOGGLE,
} TDL_LED_STATUS_E;

typedef struct { 
    uint32_t           cnt;   
    TDL_LED_STATUS_E   start_stat;  
    TDL_LED_STATUS_E   end_stat;    
    uint32_t           first_half_cycle_time; 
    uint32_t           latter_half_cycle_time; 
} TDL_LED_BLINK_CFG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Finds and returns a handle to the registered LED device by its name.
 * 
 * @param dev_name The name of the LED device to find.
 * 
 * @return Returns a handle to the LED device if found, otherwise NULL.
 */
TDL_LED_HANDLE_T tdl_led_find_dev(char *dev_name);

/**
 * @brief Opens the LED device and initializes its resources.
 * 
 * @param handle The handle to the LED device.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_open(TDL_LED_HANDLE_T handle);

/**
 * @brief Sets the LED device to a specified status (ON, OFF, or TOGGLE).
 * 
 * @param handle The handle to the LED device.
 * @param status The desired status of the LED (TDL_LED_ON, TDL_LED_OFF, or TDL_LED_TOGGLE).
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_set_status(TDL_LED_HANDLE_T handle, TDL_LED_STATUS_E status);

/**
 * @brief Starts the LED flashing with a specified half-cycle time.
 * 
 * @param handle The handle to the LED device.
 * @param half_cycle_time The duration of each half cycle in milliseconds.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_flash(TDL_LED_HANDLE_T handle, uint32_t half_cycle_time);

/**
 * @brief Starts the LED blinking with the specified configuration.
 * 
 * @param handle The handle to the LED device.
 * @param cfg A pointer to the TDL_LED_BLINK_CFG_T structure containing the blink configuration.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_blink(TDL_LED_HANDLE_T handle, TDL_LED_BLINK_CFG_T *cfg);

/**
 * @brief Closes the LED device and releases associated resources.
 * 
 * @param handle A pointer to the LED device handle.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_led_close(TDL_LED_HANDLE_T handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__TDL_LED_H__*/

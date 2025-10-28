/**
* @file tuya_led.h
* @brief TUYA LED interface
* @version 0.1
* @date 2017-11-17
*
* @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
*/

#ifndef __TUYA_LED_H
#define __TUYA_LED_H

#include "tuya_cloud_types.h"
#include "tal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_TIMER_UNINIT 0xffff

typedef PVOID_T LED_HANDLE; // led handle

/**
 * @brief Definition of LED output level
 */
typedef enum {
    OL_LOW = 0,    // output level low
    OL_HIGH,       // output level high
    OL_FLASH_LOW,  // when led flash,the level output low first.
                   // when led flash end,the the level output low.
    OL_FLASH_HIGH, // when led flash,the level output high first.
                   // when led flash end,the the level output high.
}LED_LT_E;

/**
 * @brief Create LED instance
 * 
 * @param[in] pinname Work state info, see GW_WORK_STAT_MAG_S
 * @param[in] high Default is high
 * @param[out] handle Handler of LED instance
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
 OPERATE_RET tuya_create_led_handle(IN CONST UINT_T pinname,IN CONST BOOL_T high,OUT LED_HANDLE *handle);

/**
 * @brief Create LED instance
 * 
 * @param[in] pinname Work state info, see GW_WORK_STAT_MAG_S
 * @param[in] high Default is high
 * @param[out] handle Handler of LED instance
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_create_led_handle_select(IN CONST UINT_T pinname,IN CONST BOOL_T high,OUT LED_HANDLE *handle);

/**
 * @brief Config LED light flash params
 * 
 * @param[in] handle LED handle created by tuya_create_led_handle
 * @param[in] type LED output type, see LED_LT_E
 * @param[in] flh_mstime Flash duration (unit ms)
 * @param[in] flh_ms_sumtime Total flash time (unit ms), 0xFFff indicate forever
 * 
 * @note if(OL_FLASH_XXX == type) then flh_mstime and flh_ms_sumtime is valid
 */
VOID tuya_set_led_light_type(IN CONST LED_HANDLE handle,IN CONST LED_LT_E type,
                            IN CONST USHORT_T flh_mstime,IN CONST USHORT_T flh_ms_sumtime);

#ifdef __cplusplus
}
#endif

#endif


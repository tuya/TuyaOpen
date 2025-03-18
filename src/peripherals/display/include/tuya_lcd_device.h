/**
 * @file tuya_lcd_device.h
 * @version 0.1
 * @date 2025-03-13
 */

#ifndef __TUYA_LCD_DEVICE_H__
#define __TUYA_LCD_DEVICE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register an LCD device.
 * 
 * This function initializes and registers an LCD device based on the provided device ID.
 * It configures the device's basic properties such as resolution, pixel format, rotation,
 * and backlight mode. Depending on the backlight mode, it sets up either GPIO or PWM parameters.
 * Finally, the function registers the LCD device using the configured parameters.
 * 
 * @param dev_id The unique identifier for the LCD device to be registered.
 * @return OPERATE_RET Returns 0 (OPRT_OK) on success, or a non-zero error code on failure.
 */
OPERATE_RET tuya_lcd_device_register(int dev_id);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_LCD_DEVICE_H__ */

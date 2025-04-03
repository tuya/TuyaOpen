/**
 * @file app_button.h
 * @brief app_button module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_BUTTON_H__
#define __APP_BUTTON_H__

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
OPERATE_RET app_audio_button_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BUTTON_H__ */

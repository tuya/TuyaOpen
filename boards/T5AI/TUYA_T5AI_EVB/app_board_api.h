/**
 * @file app_board_api.h
 * @brief app_board_api module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __APP_BOARD_API_H__
#define __APP_BOARD_API_H__

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
 * @brief Initialize the audio driver
 *
 * @param name Name of the audio driver
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_audio_driver_init(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BOARD_API_H__ */

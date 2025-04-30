/**
 * @file app_board_api.h
 * @brief app_board_api module is used to
 * @version 0.1
 * @date 2025-04-23
 */

#ifndef __APP_BOARD_API_H__
#define __APP_BOARD_API_H__

#include "tuya_cloud_types.h"
#include "display_common.h"

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

OPERATE_RET app_audio_driver_init(const char *name);

/**
 * @brief Initialize the display system
 *
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_display_init(void);

/**
 * @brief Send display message to the display system
 *
 * @param tp Type of the display message
 * @param data Pointer to the message data
 * @param len Length of the message data
 * @return OPERATE_RET Result of sending the message, OPRT_OK indicates success
 */
OPERATE_RET app_display_send_msg(TY_DISPLAY_TYPE_E tp, uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BOARD_API_H__ */

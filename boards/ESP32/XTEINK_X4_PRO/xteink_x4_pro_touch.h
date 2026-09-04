/**
 * @file xteink_x4_pro_touch.h
 * @brief Xteink X4 Pro GT911 capacitive touch driver (tkl_i2c/tkl_gpio only).
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_PRO_TOUCH_H__
#define __XTEINK_X4_PRO_TOUCH_H__

#include "board_com_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the GT911 touch controller on the X4 Pro.
 * @return OPRT_OK on success, OPRT_COM_ERROR if the controller does not ACK.
 */
OPERATE_RET xteink_x4_pro_touch_init(void);

/**
 * @brief Poll the current touch state (non-blocking).
 * @param[out] state non-NULL receives the snapshot.
 * @return OPRT_OK on success, OPRT_INVALID_PARM if state is NULL.
 */
OPERATE_RET xteink_x4_pro_touch_poll(X4PRO_TOUCH_STATE_T *state);

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_PRO_TOUCH_H__ */

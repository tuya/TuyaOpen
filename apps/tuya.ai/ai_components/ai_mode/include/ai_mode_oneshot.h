/**
 * @file ai_mode_oneshot.h
 * @brief AI oneshot mode module header
 *
 * This header file defines the functions for the AI oneshot mode, which
 * enables single-shot voice interaction triggered by button press.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_MODE_ONESHOT_H__
#define __AI_MODE_ONESHOT_H__

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
@brief Register oneshot mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_oneshot_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_MODE_ONESHOT_H__ */

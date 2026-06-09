/**
 * @file ai_mode_wakeup.h
 * @brief AI wakeup mode module header
 *
 * This header file defines the functions for the AI wakeup mode, which
 * enables wake word detection and automatic voice interaction.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_MODE_WAKEUP_H__
#define __AI_MODE_WAKEUP_H__

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
@brief Register wakeup mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_wakeup_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_MODE_WAKEUP_H__ */

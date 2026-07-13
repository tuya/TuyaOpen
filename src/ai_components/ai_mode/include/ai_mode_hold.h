/**
 * @file ai_mode_hold.h
 * @brief AI hold mode module header
 *
 * This header file defines the functions for the AI hold mode, which
 * enables continuous voice interaction while button is held down.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_MODE_HOLD_H__
#define __AI_MODE_HOLD_H__

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
@brief Register hold mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_hold_register(void);



#ifdef __cplusplus
}
#endif

#endif /* __AI_MODE_HOLD_H__ */

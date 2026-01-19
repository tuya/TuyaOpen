/**
 * @file ai_mode_free.h
 * @brief AI free mode module header
 *
 * This header file defines the functions for the AI free mode, which
 * enables free-form voice interaction without specific triggers.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_MODE_FREE_H__
#define __AI_MODE_FREE_H__

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
@brief Register free mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_free_register(void);


#ifdef __cplusplus
}
#endif

#endif /* __AI_MODE_FREE_H__ */

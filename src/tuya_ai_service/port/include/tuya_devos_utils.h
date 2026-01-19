/**
 * @file tuya_devos_utils.h
 * @brief tuya_devos_utils module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_DEVOS_UTILS_H__
#define __TUYA_DEVOS_UTILS_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"

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
 * @brief Get gateway's device id
 *
 * @return Device id as a string, return NULL if not exist
 */
CONST char *get_gw_dev_id(VOID);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_DEVOS_UTILS_H__ */

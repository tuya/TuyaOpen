/**
 * @file tuya_device_meta.h
 * @brief tuya_device_meta module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_DEVICE_META_H__
#define __TUYA_DEVICE_META_H__

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
 * @brief Initialize device meta module (create mutex)
 * @note Must be called once before any other tuya_device_meta_* functions
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET tuya_device_meta_init(void);

/**
 * @brief Deinitialize device meta module (release mutex)
 * @note Must be called once before exiting application
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET tuya_device_meta_deinit(void);

/**
 * @brief Add a string value to device meta (local cache)
 * @param[in] key meta key, must not be NULL
 * @param[in] value meta value, must be a string
 * @return OPRT_OK on success, OPRT_INVALID_PARM on invalid param, OPRT_MALLOC_FAILED on alloc fail
 */
OPERATE_RET tuya_device_meta_add_string(const char *key, const char *value);

/**
 * @brief Add an integer value to device meta (local cache)
 * @param[in] key meta key, must not be NULL
 * @param[in] value meta value, must be an integer
 * @return OPRT_OK on success, OPRT_INVALID_PARM on invalid param, OPRT_MALLOC_FAILED on alloc fail
 */
OPERATE_RET tuya_device_meta_add_number(const char *key, int value);

/**
 * @brief Report device meta to cloud and clear local cache on completion
 * @return OPRT_OK on success, OPRT_INVALID_PARM if not initialized, OPRT_COM_ERROR on cloud error
 */
OPERATE_RET tuya_device_meta_report(void);

/**
 * @brief Reset device meta: clear local json cache
 * @return OPRT_OK on success, OPRT_INVALID_PARM if json is not initialized
 */
OPERATE_RET tuya_device_meta_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_DEVICE_META_H__ */

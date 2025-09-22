/**
 * @file tuya_smartpointer.h
 * @brief tuya reference data module
 * @version 1.0
 * @date 2019-10-30
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _TUYA_SMARTPOINTER_H
#define _TUYA_SMARTPOINTER_H

#include "tuya_cloud_types.h"
#include "tkl_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief the reference data
 *
 */
typedef struct {
    TKL_MUTEX_HANDLE mutex;
    BOOL_T malk;
    uint32_t rfc;
    uint32_t data_len;
    void *data;
} SMARTPOINTER_T;

/**
 * @brief create a reference data
 *
 * @param[in] data the data buffer
 * @param[in] data_len the date length
 * @param[in] malk need malloc memory for the data
 * @param[in] cnt the Initial value of the reference
 * @return the reference data address
 */
SMARTPOINTER_T *tuya_smartpointer_create(void *data, const uint32_t data_len, const BOOL_T malk, const uint32_t cnt);

/**
 * @brief get the reference data, increase the reference
 *
 * @param[inout] sp_data the reference data
 * @return void
 */
void tuya_smartpointer_get(SMARTPOINTER_T *sp_data);

/**
 * @brief put the reference data, decrease the reference
 *
 * @param[inout] sp_data the reference data
 * @return void
 *
 * @note the reference data will be released when reference is 0
 */
void tuya_smartpointer_put(SMARTPOINTER_T *sp_data);

/**
 * @brief delete the reference data, ignore the reference
 *
 * @param[inout] sp_data the reference data
 * @return void
 */
void tuya_smartpointer_del(SMARTPOINTER_T *sp_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

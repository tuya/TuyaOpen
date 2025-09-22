/**
 * @file tkl_ota.h
 * @brief Common process - ota
 * @version 1.0.0
 * @date 2021-04-21
 *
 * @copyright Copyright (C) 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_OTA_H__
#define __TKL_OTA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief get ota ability
 *
 * @param[out] image_size:  max image size
 * @param[out] type:   TUYA_OTA_TYPE_E, full package or compress package use TUYA_OTA_FULL, difference ota use
 * TUYA_OTA_DIFF
 * @note This API is used for get chip ota ability
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_ota_get_ability(uint32_t *image_size, TUYA_OTA_TYPE_E *type);

/**
 * @brief ota start notify
 *
 * @param[in] image_size:  image size
 * @param[in] type:        ota type
 * @param[in] path:        ota path
 *
 * @note This API is used for ota start notify
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_ota_start_notify(uint32_t image_size, TUYA_OTA_TYPE_E type, TUYA_OTA_PATH_E path);

/**
 * @brief ota data process
 *
 * @param[in] pack:       point to ota pack
 * @param[in] remain_len: ota pack remain len
 *
 * @note This API is used for ota data process
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_ota_data_process(TUYA_OTA_DATA_T *pack, uint32_t *remain_len);

/**
 * @brief ota end notify
 *
 * @param[in] reset:  ota reset
 *
 * @note This API is used for ota end notify
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_ota_end_notify(BOOL_T reset);

/**
 * @brief get old firmware info
 *
 * @param[out] image_size:  max image size
 * @param[out] type:        ota type
 *
 * @note This API is used for old firmware info, and only used in resumes transmission at break-points
 * @note Only use for ble sub device
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_ota_get_old_firmware_info(TUYA_OTA_FIRMWARE_INFO_T **info);

#ifdef __cplusplus
}
#endif

#endif

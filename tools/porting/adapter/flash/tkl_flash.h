/**
 * @file tkl_flash.h
 * @brief Common process - adapter the flash api
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_FLASH_H__
#define __TKL_FLASH_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief read flash
 *
 * @param[in] addr: flash address
 * @param[out] dst: pointer of buffer
 * @param[in] size: size of buffer
 *
 * @note This API is used for reading flash.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_flash_read(uint32_t addr, uint8_t *dst, uint32_t size);

/**
 * @brief write flash
 *
 * @param[in] addr: flash address
 * @param[in] src: pointer of buffer
 * @param[in] size: size of buffer
 *
 * @note This API is used for writing flash.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_flash_write(uint32_t addr, const uint8_t *src, uint32_t size);

/**
 * @brief erase flash
 *
 * @param[in] addr: flash address
 * @param[in] size: size of flash block
 *
 * @note This API is used for erasing flash.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_flash_erase(uint32_t addr, uint32_t size);

/**
 * @brief lock flash
 *
 * @param[in] addr: lock begin addr
 * @param[in] size: lock area size
 *
 * @note This API is used for lock flash.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_flash_lock(uint32_t addr, uint32_t size);

/**
 * @brief unlock flash
 *
 * @param[in] addr: unlock begin addr
 * @param[in] size: unlock area size
 *
 * @note This API is used for unlock flash.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_flash_unlock(uint32_t addr, uint32_t size);

/**
 * @brief get flash information
 *
 * @param[out] info: the description of the flash
 *
 * @note This API is used to get description of storage.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_E type, TUYA_FLASH_BASE_INFO_T *info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

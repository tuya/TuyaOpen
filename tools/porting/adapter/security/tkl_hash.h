/**
 * @file tkl_hash.h
 * @brief Common process - adapter the hash api provide by OS
 * @version 0.1
 * @date 2022-04-22
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_HASH_H__
#define __TKL_HASH_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TKL_HASH_HANDLE;

/**
 * @brief This function Create&initializes a sha256 context.
 *
 * @param[out] ctx: sha256 handle
 *
 * @note This API is used to create and init sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_create_init(TKL_HASH_HANDLE *ctx);

/**
 * @brief This function clears a sha256 context.
 *
 * @param[in] ctx: sha256 handle
 *
 * @note This API is used to release sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_free(TKL_HASH_HANDLE ctx);

/**
 * @brief This function starts a sha224 or sha256 checksum
 *                 calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[in] is224:    This determines which function to use. This must be
 *                 either \c 0 for sha256, or \c 1 for sha224.
 *
 * @note This API is used to start sha256 or sha224.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_starts_ret(TKL_HASH_HANDLE ctx, int32_t is224);

/**
 * @brief This function feeds an input buffer into an ongoing
 *                 sha256 checksum calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 *
 * @note This API is used to update sha256 or sha224.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen);

/**
 * @brief This function finishes the sha256 operation, and writes
 *                 the result to the output buffer.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[out] output:   The sha224 or sha256 checksum result.
 *                 This must be a writable buffer of length \c 32 Bytes.
 *
 * @note This API is used to out sha256 result.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[32]);

/**
 * @brief This function Create&initializes a md5 context.
 *
 * @param[out] ctx: md5 handle
 *
 * @note This API is used to create and init md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_create_init(TKL_HASH_HANDLE *ctx);

/**
 * @brief This function clears a md5 context.
 *
 * @param[in] ctx: md5 handle
 *
 * @note This API is used to release md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_free(TKL_HASH_HANDLE ctx);

/**
 * @brief This function starts a md5 checksum
 *                 calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 *
 * @note This API is used to start md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_starts_ret(TKL_HASH_HANDLE ctx);

/**
 * @brief This function feeds an input buffer into an ongoing
 *                 md5 checksum calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 *
 * @note This API is used to update md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen);

/**
 * @brief This function finishes the md5 operation, and writes
 *                 the result to the output buffer.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[out] output:   The sha224 or md5 checksum result.
 *                 This must be a writable buffer of length \c 16 Bytes.
 *
 * @note This API is used to out md5 result.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[16]);

/**
 * @brief This function Create&initializes a sha1 context.
 *
 * @param[out] ctx: sha1 handle
 *
 * @note This API is used to create and init sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_create_init(TKL_HASH_HANDLE *ctx);

/**
 * @brief This function clears a sha1 context.
 *
 * @param[in] ctx: sha1 handle
 *
 * @note This API is used to release sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_free(TKL_HASH_HANDLE ctx);

/**
 * @brief This function starts a sha224 or sha1 checksum
 *                 calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 *
 * @note This API is used to start sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_starts_ret(TKL_HASH_HANDLE ctx);

/**
 * @brief This function feeds an input buffer into an ongoing
 *                 sha1 checksum calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 *
 * @note This API is used to update sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen);

/**
 * @brief This function finishes the sha1 operation, and writes
 *                 the result to the output buffer.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[out] output:   The sha224 or sha1 checksum result.
 *                 This must be a writable buffer of length \c 20 Bytes.
 *
 * @note This API is used to out sha1 result.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[20]);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

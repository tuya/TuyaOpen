/**
 * @file mbedtls_hash.c
 * @brief Provides functions for SHA256, MD5, and SHA1 hash operations using MbedTLS.
 *
 * This file contains the implementation of functions to create, initialize, update,
 * and finalize SHA256, MD5, and SHA1 hash operations. It utilizes the MbedTLS library
 * for cryptographic operations, providing a wrapper for easier usage within applications
 * that require cryptographic hashing. The functions are designed to be used with Tuya's
 * IoT platform, encapsulating common operations in a simplified API.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_iot_config.h"
#include "tkl_memory.h"
#include "tkl_hash.h"
#if !defined(ENABLE_PLATFORM_SHA256)
#include "mbedtls/sha256.h"
#endif
#if !defined(ENABLE_PLATFORM_MD5)
#include "mbedtls/md5.h"
#endif
#if !defined(ENABLE_PLATFORM_SHA1)
#include "mbedtls/sha1.h"
#endif

#if !defined(ENABLE_PLATFORM_SHA256)

/**
 * @brief This function Create&initializes a sha256 context.
 *
 * @param[out] ctx: sha256 handle
 *
 * @note This API is used to create and init sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_create_init(TKL_HASH_HANDLE *ctx)
{
    mbedtls_sha256_context *mbedtls_ctx;

    mbedtls_ctx = (mbedtls_sha256_context *)tkl_system_malloc(sizeof(mbedtls_sha256_context));
    if (mbedtls_ctx) {
        mbedtls_sha256_init(mbedtls_ctx);
    } else {
        return OPRT_MALLOC_FAILED;
    }

    *ctx = mbedtls_ctx;

    return OPRT_OK;
}

/**
 * @brief This function clears a sha256 context.
 *
 * @param[in] ctx: sha256 handle
 *
 * @note This API is used to release sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha256_free(TKL_HASH_HANDLE ctx)
{
    if (ctx != NULL) {
        mbedtls_sha256_free((mbedtls_sha256_context *)ctx);
        tkl_system_free(ctx);
    }

    return OPRT_OK;
}

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
OPERATE_RET tkl_sha256_starts_ret(TKL_HASH_HANDLE ctx, int32_t is224)
{

    if (mbedtls_sha256_starts((mbedtls_sha256_context *)ctx, is224) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

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
OPERATE_RET tkl_sha256_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen)
{
    if (mbedtls_sha256_update((mbedtls_sha256_context *)ctx, input, ilen) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

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
OPERATE_RET tkl_sha256_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[32])
{
    if (mbedtls_sha256_finish((mbedtls_sha256_context *)ctx, output) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}
#endif

#if !defined(ENABLE_PLATFORM_MD5)

/**
 * @brief This function Create&initializes a md5 context.
 *
 * @param[out] ctx: md5 handle
 *
 * @note This API is used to create and init md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_create_init(TKL_HASH_HANDLE *ctx)
{
    mbedtls_md5_context *mbedtls_ctx;

    mbedtls_ctx = (mbedtls_md5_context *)tkl_system_malloc(sizeof(mbedtls_md5_context));
    if (mbedtls_ctx) {
        mbedtls_md5_init(mbedtls_ctx);
    } else {
        return OPRT_MALLOC_FAILED;
    }

    *ctx = mbedtls_ctx;

    return OPRT_OK;
}

/**
 * @brief This function clears a md5 context.
 *
 * @param[in] ctx: md5 handle
 *
 * @note This API is used to release md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_md5_free(TKL_HASH_HANDLE ctx)
{
    if (ctx != NULL) {
        mbedtls_md5_free((mbedtls_md5_context *)ctx);
        tkl_system_free(ctx);
    }

    return OPRT_OK;
}

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
OPERATE_RET tkl_md5_starts_ret(TKL_HASH_HANDLE ctx)
{
    if (mbedtls_md5_starts((mbedtls_md5_context *)ctx) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

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
OPERATE_RET tkl_md5_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen)
{
    if (mbedtls_md5_update((mbedtls_md5_context *)ctx, input, ilen) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

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
OPERATE_RET tkl_md5_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[16])
{
    if (mbedtls_md5_finish((mbedtls_md5_context *)ctx, output) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

#endif

#if !defined(ENABLE_PLATFORM_SHA1)

/**
 * @brief This function Create&initializes a sha1 context.
 *
 * @param[out] ctx: sha1 handle
 *
 * @note This API is used to create and init sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_create_init(TKL_HASH_HANDLE *ctx)
{
    mbedtls_sha1_context *mbedtls_ctx;

    mbedtls_ctx = (mbedtls_sha1_context *)tkl_system_malloc(sizeof(mbedtls_sha1_context));
    if (mbedtls_ctx) {
        mbedtls_sha1_init(mbedtls_ctx);
    } else {
        return OPRT_MALLOC_FAILED;
    }

    *ctx = mbedtls_ctx;

    return OPRT_OK;
}

/**
 * @brief This function clears a sha1 context.
 *
 * @param[in] ctx: sha1 handle
 *
 * @note This API is used to release sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_free(TKL_HASH_HANDLE ctx)
{
    if (ctx != NULL) {
        mbedtls_sha1_free((mbedtls_sha1_context *)ctx);
        tkl_system_free(ctx);
    }

    return OPRT_OK;
}

/**
 * @brief This function starts a sha1 checksum
 *                 calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 *
 * @note This API is used to start sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_starts_ret(TKL_HASH_HANDLE ctx)
{
    if (mbedtls_sha1_starts((mbedtls_sha1_context *)ctx) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

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
OPERATE_RET tkl_sha1_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen)
{
    if (mbedtls_sha1_update((mbedtls_sha1_context *)ctx, input, ilen) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

/**
 * @brief This function finishes the sha1 operation, and writes
 *                 the result to the output buffer.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 * @param[out] output:   The sha224 or sha1 checksum result.
 *                 This must be a writable buffer of length \c 16 Bytes.
 *
 * @note This API is used to out sha1 result.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_sha1_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[20])
{
    if (mbedtls_sha1_finish((mbedtls_sha1_context *)ctx, (unsigned char *)output) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

#endif
/**
 * @file tal_hash.h
 * @brief Hash and HMAC utilities for Tuya SDK.
 *
 * This header file defines the interfaces for hash and HMAC (Hash-based Message
 * Authentication Code) operations within the Tuya SDK. It provides a layer of
 * abstraction over the underlying cryptographic library, making it easier to
 * perform secure hash operations and HMAC calculations. The file includes
 * definitions for a hash MAC context structure, which encapsulates the context
 * needed for HMAC operations, including the inner and outer padding.
 *
 * The functions defined in this file allow for the creation and initialization
 * of SHA256 contexts, which are essential for generating secure hash values.
 * These operations are crucial for ensuring data integrity and authenticity in
 * Tuya-based applications, especially in scenarios involving secure
 * communication and data storage.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It abstracts complex cryptographic
 * operations, providing a simplified interface for developers working on Tuya's
 * IoT platform.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_HASH_H__
#define __TAL_HASH_H__

#include "tkl_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TKL_HASH_HANDLE ctx;
    uint8_t ipad[64]; /*!< HMAC: inner padding */
    uint8_t opad[64]; /*!< HMAC: outer padding */
} tal_hash_mac_context_t;

/**
 * @brief This function Create&initializes a sha256 context.
 *
 * @param[out] ctx: sha256 handle
 *
 * @note This API is used to create and init sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_create_init(TKL_HASH_HANDLE *ctx);

/**
 * @brief This function clears a sha256 context.
 *
 * @param[in] ctx: sha256 handle
 *
 * @note This API is used to release sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_free(TKL_HASH_HANDLE ctx);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_starts_ret(TKL_HASH_HANDLE ctx, int32_t is224);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[32]);

/**
 * @brief This function Create&initializes a md5 context.
 *
 * @param[out] ctx: md5 handle
 *
 * @note This API is used to create and init md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_md5_create_init(TKL_HASH_HANDLE *ctx);

/**
 * @brief This function clears a md5 context.
 *
 * @param[in] ctx: md5 handle
 *
 * @note This API is used to release md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_md5_free(TKL_HASH_HANDLE ctx);

/**
 * @brief This function starts a md5 checksum
 *                 calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 *
 * @note This API is used to start md5.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_md5_starts_ret(TKL_HASH_HANDLE ctx);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_md5_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_md5_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[16]);

/**
 * @brief This function Create&initializes a sha1 context.
 *
 * @param[out] ctx: sha1 handle
 *
 * @note This API is used to create and init sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_create_init(TKL_HASH_HANDLE *ctx);

/**
 * @brief This function clears a sha1 context.
 *
 * @param[in] ctx: sha1 handle
 *
 * @note This API is used to release sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_free(TKL_HASH_HANDLE ctx);

/**
 * @brief This function starts a sha1 checksum
 *                 calculation.
 *
 * @param[in] ctx: The context to use. This must be initialized.
 *
 * @note This API is used to start sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_starts_ret(TKL_HASH_HANDLE ctx);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_update_ret(TKL_HASH_HANDLE ctx, const uint8_t *input, size_t ilen);

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
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_finish_ret(TKL_HASH_HANDLE ctx, uint8_t output[20]);
/**
 * @brief          This function calculates the SHA-224 or SHA-256
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-256 result is calculated as
 *                 output = SHA-256(input buffer).
 *
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 * @param[out] output:   The SHA-224 or SHA-256 checksum result. This must
 *                 be a writable buffer of length \c 32 Bytes.
 * @param[in] is224:    Determines which function to use. This must be
 *                 either \c 0 for SHA-256, or \c 1 for SHA-224.
 *
 * @return         OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_ret(const uint8_t *input, size_t ilen, uint8_t output[32], int32_t is224);

/**
 * @brief          Output = MD5( input buffer )
 *
 * @param[in] input:    buffer holding the data
 * @param[in] ilen:     length of the input data
 * @param[out] output:   MD5 checksum result
 *
 * @return         OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 */
OPERATE_RET tal_md5_ret(const uint8_t *input, size_t ilen, uint8_t output[16]);

/**
 * @brief          Output = sha1( input buffer )
 *
 * @param[in] input:    buffer holding the data
 * @param[in] ilen:     length of the input data
 * @param[out] output:   sha1 checksum result
 *
 * @return         OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 */
OPERATE_RET tal_sha1_ret(const uint8_t *input, size_t ilen, uint8_t output[20]);
/**
 * @brief This function Create&initializes a sha256 maccontext.
 *
 * @param[out] hmac_handle: sha256_mac handle
 *
 * @note This API is used to create and init sha256_mac.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_mac_create_init(tal_hash_mac_context_t *hmac_handle);
/**
 * @brief This function clears a sha256 mac context.
 *
 * @param[in] hmac_handle: sha256 mac handle
 *
 * @note This API is used to release sha256.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_mac_free(tal_hash_mac_context_t *hmac_handle);

/**
 * @brief This function starts a sha256 mac checksum
 *                 calculation.
 *
 * @param[in] hmac_handle: The context to use. This must be initialized.
 * @param[in] key:    key
 * @param[in] keylen: keylen
 *
 * @note This API is used to start sha256 mac.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_mac_starts(tal_hash_mac_context_t *hmac_handle, const uint8_t *key, size_t keylen);
/**
 * @brief This function feeds an input buffer into an ongoing
 *                 sha256 mac checksum calculation.
 *
 * @param[in] hmac_handle: The context to use. This must be initialized.
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 *
 * @note This API is used to update sha256 mac.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_mac_update(tal_hash_mac_context_t *hmac_handle, const uint8_t *input, size_t ilen);
/**
 * @brief This function finishes the sha256 mac operation, and writes
 *                 the result to the output buffer.
 *
 * @param[in] hmac_handle: The context to use. This must be initialized.
 * @param[out] output:   The sha256 mac checksum result.
 *                 This must be a writable buffer of length \c 32 Bytes.
 *
 * @note This API is used to out sha256 mac result.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_mac_finish(tal_hash_mac_context_t *hmac_handle, uint8_t *output);

/**
 * @brief          This function calculates the SHA-256 MAC
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-256 result is calculated as
 *                 output = SHA-256 MAC.
 *
 * @param[in] key:    The buffer holding the data. This must be a readable
 *                 buffer of length \p keylen Bytes.
 * @param[in] keylen:     The length of the key data in Bytes.*
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 * @param[out] output:   The SHA-256 MAC checksum result. This must
 *                 be a writable buffer of length \c 32 Bytes.
 *
 * @return         OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha256_mac(const uint8_t *key, size_t keylen, const uint8_t *input, size_t ilen, uint8_t *output);

/**
 * @brief This function Create&initializes a sha1 maccontext.
 *
 * @param[out] hmac_handle: sha1_mac handle
 *
 * @note This API is used to create and init sha1_mac.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_mac_create_init(tal_hash_mac_context_t *hmac_handle);
/**
 * @brief This function clears a sha1 mac context.
 *
 * @param[in] hmac_handle: sha1 mac handle
 *
 * @note This API is used to release sha1.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_mac_free(tal_hash_mac_context_t *hmac_handle);

/**
 * @brief This function starts a sha1 mac checksum
 *                 calculation.
 *
 * @param[in] hmac_handle: The context to use. This must be initialized.
 * @param[in] key:    key
 * @param[in] keylen: keylen
 *
 * @note This API is used to start sha1 mac.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_mac_starts(tal_hash_mac_context_t *hmac_handle, const uint8_t *key, size_t keylen);
/**
 * @brief This function feeds an input buffer into an ongoing
 *                 sha1 mac checksum calculation.
 *
 * @param[in] hmac_handle: The context to use. This must be initialized.
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 *
 * @note This API is used to update sha1 mac.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_mac_update(tal_hash_mac_context_t *hmac_handle, const uint8_t *input, size_t ilen);
/**
 * @brief This function finishes the sha1 mac operation, and writes
 *                 the result to the output buffer.
 *
 * @param[in] hmac_handle: The context to use. This must be initialized.
 * @param[out] output:   The sha1 mac checksum result.
 *                 This must be a writable buffer of length \c 32 Bytes.
 *
 * @note This API is used to out sha1 mac result.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_mac_finish(tal_hash_mac_context_t *hmac_handle, uint8_t *output);

/**
 * @brief          This function calculates the SHA-256 MAC
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-256 result is calculated as
 *                 output = SHA-256 MAC.
 *
 * @param[in] key:    The buffer holding the data. This must be a readable
 *                 buffer of length \p keylen Bytes.
 * @param[in] keylen:     The length of the key data in Bytes.*
 * @param[in] input:    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * @param[in] ilen:     The length of the input data in Bytes.
 * @param[out] output:   The SHA-256 MAC checksum result. This must
 *                 be a writable buffer of length \c 32 Bytes.
 *
 * @return         OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_sha1_mac(const uint8_t *key, size_t keylen, const uint8_t *input, size_t ilen, uint8_t *output);

/**
 * @brief Performs a self-test for the SHA256 algorithm.
 *
 * This function performs a self-test for the SHA256 algorithm and returns the
 * result.
 *
 * @param verbose An integer value indicating whether to display verbose output
 * during the self-test. A non-zero value enables verbose output, while a zero
 * value disables it.
 *
 * @return An OPERATE_RET value indicating the result of the self-test.
 *         - OPRT_OK if the self-test passed successfully.
 *         - Other error codes if the self-test failed.
 */
OPERATE_RET tal_sha256_self_test(int32_t verbose);

/**
 * @brief Performs a self-test for the MD5 hash algorithm.
 *
 * This function is used to perform a self-test for the MD5 hash algorithm.
 * It takes an optional verbose parameter to control the level of output during
 * the test.
 *
 * @param verbose The level of output during the self-test. Set to 0 for no
 * output, or a positive value for verbose output.
 * @return The result of the self-test. Returns OPERATE_RET_OK if the self-test
 * passes, or an error code if it fails.
 */
OPERATE_RET tal_md5_self_test(int32_t verbose);

/**
 * @brief Performs a self-test for the SHA1 algorithm.
 *
 * This function performs a self-test for the SHA1 algorithm and returns the
 * result.
 *
 * @param verbose An integer value indicating whether to display verbose output
 * during the self-test. A non-zero value enables verbose output, while a zero
 * value disables it.
 *
 * @return An OPERATE_RET value indicating the result of the self-test.
 *         If the self-test is successful, it returns OPERATE_RET_OK.
 *         Otherwise, it returns an error code indicating the reason for the
 * failure.
 */
OPERATE_RET tal_sha1_self_test(int32_t verbose);

/**
 * @brief Performs a self-test for the SHA256 MAC algorithm.
 *
 * This function performs a self-test for the SHA256 MAC algorithm and returns
 * the result.
 *
 * @param verbose An integer value indicating whether to display verbose output
 * during the self-test. A non-zero value enables verbose output, while a zero
 * value disables it.
 *
 * @return An OPERATE_RET value indicating the result of the self-test.
 *         If the self-test is successful, it returns OPERATE_RET_OK.
 *         Otherwise, it returns an error code indicating the reason for the
 * failure.
 */
OPERATE_RET tal_sha256_mac_self_test(int32_t verbose);

/**
 * @brief Performs a self-test for the SHA1 MAC algorithm.
 *
 * This function performs a self-test for the SHA1 MAC algorithm and returns the
 * result.
 *
 * @param verbose An integer value indicating whether to display verbose output
 * during the self-test. A non-zero value enables verbose output, while a zero
 * value disables it.
 *
 * @return An OPERATE_RET value indicating the result of the self-test.
 *         Possible return values are:
 *         - OPRT_OK: The self-test was successful.
 *         - Other error codes: The self-test failed.
 */
OPERATE_RET tal_sha1_mac_self_test(int32_t verbose);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

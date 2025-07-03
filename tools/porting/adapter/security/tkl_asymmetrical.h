/**
 * @file tal_asymmetrical.h
 * @brief Common process - adapter the asymmetrical api  provide by OS
 * @version 0.1
 * @date 2022-04-22
 *
 * @copyright Copyright 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_ASYMMETRICAL_H__
#define __TKL_ASYMMETRICAL_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TKL_ASYMMETRICAY_HANDLE;

/**
 * @brief This function Create&initializes a rsa context.
 *
 * @param[out] ctx: rsa handle
 *
 * @note This API is used to create and init rsa.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rsa_create_init(TKL_ASYMMETRICAY_HANDLE *ctx);

/**
 * @brief This function releases and clears the specified rsa context.
 *
 * @param[in] ctx: The rsa context to clear.
 *
 * @note This API is used to release rsa.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rsa_free(TKL_ASYMMETRICAY_HANDLE ctx);

/**
 * @brief          This function generates an RSA keypair.
 *
 * @note           tkl_rsa_create_init() must be called before this function,
 *                 to set up the RSA context.
 *
 * @param ctx      The initialized RSA context used to hold the key.
 * @param nbits    The size of the public key in bits.
 * @param exponent The public exponent to use. For example, \c 65537.
 *                 This must be odd and greater than \c 1.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rsa_gen_key(TKL_ASYMMETRICAY_HANDLE ctx, uint32_t nbits, int32_t exponent);

/**
 * @brief           Write a private key to a PKCS#1 or SEC1 DER structure
 *                  Note: data is written at the end of the buffer! Use the
 *                        return value to determine where you should start
 *                        using the buffer
 *
 * @param ctx       RSA context which must contain a valid private key.
 * @param buf       buffer to write to
 * @param size      size of the buffer
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rsa_write_key_der(TKL_ASYMMETRICAY_HANDLE ctx, uint8_t *buf, size_t size);

/**
 * @brief          This function performs an RSA public key operation.
 *
 * @param ctx      The initialized RSA context to use.
 * @param input    The input buffer. This must be a readable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * @param output   The output buffer. This must be a writable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * @note           This function does not handle message padding.
 *
 * @note           Make sure to set \p input[0] = 0 or ensure that
 *                 input is smaller than \p N.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_rsa_public(TKL_ASYMMETRICAY_HANDLE ctx, const uint8_t *input, uint8_t *output);

/**
 * @brief          This function performs an RSA private key operation.
 *
 * @note           Blinding is used if and only if a PRNG is provided.
 *
 * @note           If blinding is used, both the base of exponentation
 *                 and the exponent are blinded, providing protection
 *                 against some side-channel attacks.
 *
 * @warning        It is deprecated and a security risk to not provide
 *                 a PRNG here and thereby prevent the use of blinding.
 *                 Future versions of the library may enforce the presence
 *                 of a PRNG.
 *
 * @param ctx      The initialized RSA context to use.
 * @param input    The input buffer. This must be a readable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * @param output   The output buffer. This must be a writable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
OPERATE_RET tkl_rsa_private(TKL_ASYMMETRICAY_HANDLE ctx, const uint8_t *input, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif

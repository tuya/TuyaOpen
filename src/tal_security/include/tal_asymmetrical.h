/**
 * @file tal_asymmetrical.h
 * @brief Asymmetrical cryptography utilities for Tuya SDK.
 *
 * This header file provides the definitions and function prototypes for
 * asymmetrical cryptography operations within the Tuya SDK. It includes support
 * for key generation, conversion between different key formats, and other
 * cryptographic operations using the mbedtls library. The file defines macros
 * for specifying output modes (none, private, public) and formats (PEM, DER)
 * for cryptographic keys.
 *
 * The functions provided in this file allow for the generation of cryptographic
 * keys using RSA or ECC algorithms, conversion of mbedtls key contexts to and
 * from buffers in specified formats, and other utilities necessary for secure
 * communication and data protection in Tuya-based applications.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It relies on the mbedtls library for
 * cryptographic operations and is designed to be compatible with Tuya's IoT and
 * security protocols.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TAL_ASYMMETRICAL_H__
#define __TAL_ASYMMETRICAL_H__

#include "tkl_asymmetrical.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_iot_config.h"
#include "tuya_tls.h"
#if defined(MBEDTLS_CONFIG_FILE)
/* Load the platform configuration before PSA/private headers. mbedTLS 4.x
 * defines the external RNG context from this configuration. */
#include MBEDTLS_CONFIG_FILE
#endif
#include "mbedtls/build_info.h"
#include "mbedtls/rsa.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/pk.h"

#define OUTPUT_MODE_NONE    0
#define OUTPUT_MODE_PRIVATE 1
#define OUTPUT_MODE_PUBLIC  2

#define OUTPUT_FORMAT_PEM 0
#define OUTPUT_FORMAT_DER 1

/**
 * Generates a key pair for asymmetric encryption.
 *
 * @param type      The type of key pair to generate.
 * @param grp_id    The identifier of the elliptic curve group (for EC keys).
 * @param rsa_keysize The size of the RSA key (for RSA keys).
 * @param key       A pointer to the `mbedtls_pk_context` structure to store the
 * generated key pair.
 *
 * @return          0 on success, or a negative error code on failure.
 */
/* mbedtls 4.x removed mbedtls_pk_type_t and the legacy pk setup API.
 * PSA-based reimplementation pending; no in-tree callers today. */
#if MBEDTLS_VERSION_MAJOR < 4
int tal_gen_key(mbedtls_pk_type_t type, mbedtls_ecp_group_id grp_id, int rsa_keysize, mbedtls_pk_context *key);
#endif
/**
 * @brief Converts a public key to a specified output format and stores it in a
 * buffer.
 *
 * This function takes a public key represented by the `key` parameter and
 * converts it to the specified `output_format`. The converted key is then
 * stored in the `output_buf` buffer. The `output_mode` parameter determines the
 * behavior of the conversion.
 *
 * @param key The mbedtls_pk_context structure representing the public key to be
 * converted.
 * @param output_mode The mode of the conversion. This parameter determines the
 * behavior of the conversion.
 * @param output_format The format in which the key should be converted.
 * @param output_buf The buffer where the converted key will be stored.
 * @param output_buf_len The length of the output buffer.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tal_Pk_Convert_Buf(mbedtls_pk_context *key, int output_mode, int output_format, uint8_t *output_buf,
                       uint32_t output_buf_len);
/**
 * @brief Converts a buffer containing a public key to an mbedtls_pk_context
 * structure.
 *
 * @param in_buf The input buffer containing the public key.
 * @param intput_mode The input mode for the conversion.
 * @param input_format The input format of the public key.
 * @param key The mbedtls_pk_context structure to store the converted key.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int tal_Buf_Convert_Pk(const char *in_buf, int intput_mode, int input_format, mbedtls_pk_context *key);

/**
 * @brief This function is used to test the ty_asymmetric functionality.
 *
 * @return Returns an integer value indicating the success or failure of the
 * test.
 */
int test_ty_asymmetric(void);

#ifdef __cplusplus
}
#endif

#endif

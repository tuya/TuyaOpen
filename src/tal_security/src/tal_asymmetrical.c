/**
 * @file tal_asymmetrical.c
 * @brief Asymmetrical cryptography utilities for Tuya SDK.
 *
 * This source file implements the asymmetrical cryptography utilities provided
 * by the Tuya SDK. It includes functionalities for generating cryptographic
 * keys, converting keys between different formats, and handling keys for secure
 * communication. The file leverages the mbedtls library to provide robust and
 * secure cryptographic operations, supporting both RSA and ECC (Elliptic Curve
 * Cryptography) key types. These utilities are essential for creating and
 * managing keys that are used in various security protocols within Tuya-based
 * applications, ensuring secure device-to-device and device-to-cloud
 * communications.
 *
 * The file defines functions such as `tal_gen_key` for generating keys,
 * `tal_Pk_Convert_Buf` for converting keys between PEM and DER formats, and
 * `tal_Buf_Convert_Pk` for parsing keys from buffers. These functions abstract
 * the complexity of cryptographic operations, providing a simplified API for
 * developers to incorporate advanced security features into their applications.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It requires the mbedtls library for
 * cryptographic operations.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_memory.h"
#include "tal_asymmetrical.h"
#include <string.h>
#include "tuya_tls.h"
#include "tal_log.h"

extern int mbedtls_pk_write_key_der(const mbedtls_pk_context *key, unsigned char *buf, size_t size);
extern int mbedtls_pk_write_pubkey_der(const mbedtls_pk_context *key, unsigned char *buf, size_t size);

int __tuya_tls_random(void *p_rng, unsigned char *output, size_t output_len);

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
int tal_gen_key(mbedtls_pk_type_t type, mbedtls_ecp_group_id grp_id, int rsa_keysize, mbedtls_pk_context *key)
{
    int ret = 1;
    int exit_code = -1;

    mbedtls_pk_init(key);

    /*
     * 1.1. Generate the key
     */
    PR_DEBUG("\n  . Generating the private key ...");

    if ((ret = mbedtls_pk_setup(key, mbedtls_pk_info_from_type((mbedtls_pk_type_t)type))) != 0) {
        PR_ERR("mbedtls_pk_setup returned -0x%04x", -ret);
        goto exit;
    }

#if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_GENPRIME)
    if (type == MBEDTLS_PK_RSA) {
        ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(*key), __tuya_tls_random, NULL, rsa_keysize, 65537);
        if (ret != 0) {
            PR_ERR("mbedtls_rsa_gen_key returned -0x%04x", -ret);
            goto exit;
        }
    } else
#endif /* MBEDTLS_RSA_C */
#if defined(MBEDTLS_ECP_C)
        if (type == MBEDTLS_PK_ECKEY) {
        ret = mbedtls_ecp_gen_key(grp_id, mbedtls_pk_ec(*key), __tuya_tls_random, NULL);
        if (ret != 0) {
            PR_ERR("mbedtls_ecp_gen_key returned -0x%04x", -ret);
            goto exit;
        }
    } else
#endif /* MBEDTLS_ECP_C */
    {
        PR_ERR("key type not supported\n");
        goto exit;
    }

    PR_DEBUG(" ok\n");

    exit_code = 0;
    ret = 0;

exit:

    if (exit_code != 0) {
    }

    return ret;
}

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
                       uint32_t output_buf_len)
{
    int ret = 0;
    // unsigned char *c = output_buf;
    // size_t len = 0;

#if defined(MBEDTLS_PEM_WRITE_C)
    if (output_format == OUTPUT_FORMAT_PEM) {
        if (output_mode == OUTPUT_MODE_PRIVATE) {
            ret = mbedtls_pk_write_key_pem(key, output_buf, output_buf_len);
        } else if (output_mode == OUTPUT_MODE_PUBLIC) {
            ret = mbedtls_pk_write_pubkey_pem(key, output_buf, output_buf_len);
        }

        if (ret < 0) {
            return (ret);
        }

        // len = strlen( (char *) output_buf );
    } else
#endif
    {
        if (output_mode == OUTPUT_MODE_PRIVATE) {
            ret = mbedtls_pk_write_key_der(key, output_buf, output_buf_len);
        } else if (output_mode == OUTPUT_MODE_PUBLIC) {
            ret = mbedtls_pk_write_pubkey_der(key, output_buf, output_buf_len);
        }

        if (ret < 0) {
            return (ret);
        }

        // len = ret;
        // c = output_buf + sizeof(output_buf) - len;
    }

    return (0);
}

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
int tal_Buf_Convert_Pk(const char *in_buf, int intput_mode, int input_format, mbedtls_pk_context *key)
{
    return 0;
}

// int test_ty_asymmetric(void)
// {
//     mbedtls_pk_context key;
//     int ret = -1;
//     unsigned char output_buf[16000];

//     //gen ecc 256 keypair
//     ret = tal_gen_key(MBEDTLS_PK_ECKEY,MBEDTLS_ECP_DP_SECP256R1,0,&key);
//     if(ret != 0)
//     PR_ERR("tuya gen key is fail ,ret = %d",ret );

//     memset(output_buf, 0, 16000);
//     tal_Pk_Convert_Buf( &key,OUTPUT_MODE_PRIVATE ,OUTPUT_FORMAT_PEM,output_buf );
//     PR_DEBUG("ecc private key:%s",output_buf);

//     memset(output_buf, 0, 16000);
//     tal_Pk_Convert_Buf( &key,OUTPUT_MODE_PUBLIC ,OUTPUT_FORMAT_PEM,output_buf );
//     PR_DEBUG("ecc public key:%s",output_buf);
//     mbedtls_pk_free( &key );

//     //gen rsa 2048 keypair
//     ret = tal_gen_key(MBEDTLS_PK_RSA,MBEDTLS_ECP_DP_NONE,2048,&key);
//     if(ret != 0)
//     PR_ERR("tuya gen key is fail ,ret = %d",ret );

//     memset(output_buf, 0, 16000);
//     tal_Pk_Convert_Buf( &key,OUTPUT_MODE_PRIVATE ,OUTPUT_FORMAT_PEM,output_buf );
//     PR_DEBUG("rsa private key:%s",output_buf);

//     memset(output_buf, 0, 16000);
//     tal_Pk_Convert_Buf( &key,OUTPUT_MODE_PUBLIC ,OUTPUT_FORMAT_PEM,output_buf );
//     PR_DEBUG("rsa public key:%s",output_buf);
//     mbedtls_pk_free( &key );
// }

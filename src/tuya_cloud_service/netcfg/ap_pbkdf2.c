/**
 * @file ap_pbkdf2.c
 * @brief Implementation of the PBKDF2 (Password-Based Key Derivation Function
 * 2) algorithm with SHA-256 hashing for Tuya devices.
 *
 * This file contains the implementation of the PBKDF2 algorithm using SHA-256
 * hashing as specified in RFC 2898. It provides functions to derive a secure
 * key from a passphrase and a salt. This key derivation process is used in
 * various security contexts within the Tuya IoT SDK to enhance the security of
 * password storage and to securely generate encryption keys from user-provided
 * passwords.
 *
 * The implementation leverages the mbedtls cryptographic library to perform the
 * SHA-256 hashing and the PBKDF2 algorithm. It includes a generic pbkdf2_sha256
 * function for key derivation and a specific ap_pbkdf2_cacl function tailored
 * for use cases within the Tuya IoT SDK.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"
#include "tal_log.h"

/**
 * @brief Performs the PBKDF2 key derivation function using SHA256 as the
 * underlying hash function.
 *
 * This function takes a passphrase, salt, and other parameters to derive a key
 * using the PBKDF2 algorithm.
 *
 * @param passphrase The passphrase used as input for key derivation.
 * @param passphrase_len The length of the passphrase.
 * @param salt The salt used as input for key derivation.
 * @param salt_len The length of the salt.
 * @param iterations The number of iterations to perform in the PBKDF2
 * algorithm.
 * @param key_length The desired length of the derived key.
 * @param buf The buffer to store the derived key.
 * @param buflen The length of the buffer.
 *
 * @return 0 if the key derivation is successful, or a negative value if an
 * error occurs.
 */
int pbkdf2_sha256(const char *passphrase, size_t passphrase_len, const char *salt, size_t salt_len, int iterations,
                  uint32_t key_length, unsigned char *buf, size_t buflen)

{
    int ret;

    if (NULL == passphrase || NULL == salt) {
        return -1;
    }

    if (buflen < key_length) {
        return -1;
    }

#if (MBEDTLS_VERSION_NUMBER <= 0x03010000)
    mbedtls_md_context_t sha_ctx;
    const mbedtls_md_info_t *info_sha;
    mbedtls_md_init(&sha_ctx);

    info_sha = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info_sha == NULL) {
        ret = -1;
        goto exit;
    }

    if ((ret = mbedtls_md_setup(&sha_ctx, info_sha, 1)) != 0) {
        ret = -1;
        goto exit;
    }

    ret = mbedtls_pkcs5_pbkdf2_hmac(&sha_ctx, (const unsigned char *)passphrase, passphrase_len,
                                    (const unsigned char *)salt, salt_len, iterations, key_length, buf);
    if (ret != 0) {
        ret = -1;
        goto exit;
    }

exit:
    mbedtls_md_free(&sha_ctx);
#else
    ret = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, (const unsigned char *)passphrase, passphrase_len,
                                        (const unsigned char *)salt, salt_len, iterations, key_length, buf);
#endif

    return ret;
}

/**
 * Calculates the AP PBKDF2 (Password-Based Key Derivation Function 2) value.
 *
 * This function takes a PIN (Personal Identification Number) and a UUID
 * (Universally Unique Identifier) and calculates the PBKDF2 value using these
 * inputs. The result is stored in the provided buffer.
 * @param pin The PIN to be used for PBKDF2 calculation.
 * @param uuid The UUID to be used for PBKDF2 calculation.
 * @param buf The buffer to store the calculated PBKDF2 value.
 * @param buflen The length of the buffer.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int ap_pbkdf2_cacl(char *pin, char *uuid, uint8_t *buf, uint8_t buflen)
{
    return pbkdf2_sha256(pin, strlen(pin), uuid, strlen(uuid), 1024, 37, (unsigned char *)buf, buflen);
}

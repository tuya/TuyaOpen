#ifndef __CRYPTO_WRAPPER_H_
#define __CRYPTO_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mbedtls/version.h"
#if defined(MBEDTLS_CONFIG_FILE)
/* mbedTLS 4.x's PSA platform header is reached while build_info.h is being
 * loaded. Read the target config first so its external RNG context is defined
 * before the PSA include guard is set. */
#include MBEDTLS_CONFIG_FILE
#endif
#include "mbedtls/platform.h"
#include "mbedtls/cipher.h"
#include "mbedtls/md.h"

typedef struct {
    unsigned char *key;
    unsigned char *nonce;
    unsigned char *ad;
    unsigned char *data;
    size_t key_len;
    size_t nonce_len;
    size_t ad_len;
    size_t data_len;
    mbedtls_cipher_type_t cipher_type;
} cipher_params_t;

int mbedtls_cipher_auth_encrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len);

int mbedtls_cipher_auth_decrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len);

int mbedtls_message_digest(mbedtls_md_type_t md_type, const uint8_t *input, size_t ilen, uint8_t *digest);

int mbedtls_message_digest_hmac(mbedtls_md_type_t md_type, const uint8_t *key, size_t keylen, const uint8_t *input,
                                size_t ilen, uint8_t *digest);

/* RFC 5869 HKDF with SHA-256. mbedtls 4.x removed mbedtls_hkdf(); this
 * portable implementation covers both 3.x and 4.x. */
int mbedtls_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                        const uint8_t *info, size_t info_len, uint8_t *okm, size_t okm_len);

#ifdef __cplusplus
}
#endif
#endif

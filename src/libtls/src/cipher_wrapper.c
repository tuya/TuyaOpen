// https://tls.mbed.org/module-level-design-cipher
#include "cipher_wrapper.h"
#include "tal_log.h"
#include "tal_memory.h"

#if MBEDTLS_VERSION_NUMBER >= 0x04000000
#include "psa/crypto.h"
#endif

int mbedtls_cipher_auth_encrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len)
{
    if (input == NULL || output == NULL || olen == NULL) {
        return OPRT_INVALID_PARM;
    }

    int ret = OPRT_OK;
    unsigned char *enc_tmpbuf = NULL;
    mbedtls_cipher_info_t *cipher_info;
    mbedtls_cipher_context_t cipher_ctx;

    mbedtls_cipher_init(&cipher_ctx);

    /*
     * Read the Cipher and MD from the command line
     */
    cipher_info = (mbedtls_cipher_info_t *)mbedtls_cipher_info_from_type(input->cipher_type);
    if (cipher_info == NULL) {
        PR_ERR("Cipher not found\n");
        ret = OPRT_INVALID_PARM;
        goto EXIT;
    }

    if ((ret = mbedtls_cipher_setup(&cipher_ctx, cipher_info)) != 0) {
        PR_ERR("mbedtls_cipher_setup failed\n");
        goto EXIT;
    }

    if ((input->key_len * 8) != mbedtls_cipher_info_get_key_bitlen(cipher_info)) {
        PR_ERR("key_len:%d mbedtls_key_bitlen:%d", input->key_len * 8, mbedtls_cipher_info_get_key_bitlen(cipher_info));
        ret = OPRT_INVALID_PARM;
        goto EXIT;
    }

    if ((ret = mbedtls_cipher_setkey(&cipher_ctx, input->key, mbedtls_cipher_info_get_key_bitlen(cipher_info),
                                     MBEDTLS_ENCRYPT)) != 0) {
        PR_ERR("mbedtls_cipher_setkey() returned error\n");
        goto EXIT;
    }

    enc_tmpbuf = tal_malloc(input->data_len + tag_len);

    /*
     * Encrypt and write the ciphertext.
     */
    ret = mbedtls_cipher_auth_encrypt_ext(&cipher_ctx, input->nonce, input->nonce_len, input->ad, input->ad_len,
                                          input->data, input->data_len, enc_tmpbuf, input->data_len + tag_len, olen,
                                          tag_len);

    /* https://github.com/Mbed-TLS/mbedtls/issues/3665 */
    *olen -= tag_len;
    memcpy(output, enc_tmpbuf, *olen);
    memcpy(tag, enc_tmpbuf + (*olen), tag_len);

EXIT:
    mbedtls_cipher_free(&cipher_ctx);
    if (NULL != enc_tmpbuf) {
        tal_free(enc_tmpbuf);
        enc_tmpbuf = NULL;
    }
    return (ret);
}

int mbedtls_cipher_auth_decrypt_wrapper(const cipher_params_t *input, unsigned char *output, size_t *olen,
                                        unsigned char *tag, size_t tag_len)
{
    if (input == NULL || output == NULL || olen == NULL) {
        return OPRT_INVALID_PARM;
    }

    int ret = OPRT_OK;
    unsigned char *dec_tmpbuf = NULL;
    const mbedtls_cipher_info_t *cipher_info;
    mbedtls_cipher_context_t cipher_ctx;

    mbedtls_cipher_init(&cipher_ctx);

    /*
     * Read the Cipher and MD from the command line
     */
    cipher_info = mbedtls_cipher_info_from_type(input->cipher_type);
    if (cipher_info == NULL) {
        PR_ERR("Cipher '%s' not found\n", "mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_CBC)");
        goto EXIT;
    }

    if ((ret = mbedtls_cipher_setup(&cipher_ctx, cipher_info)) != 0) {
        PR_ERR("mbedtls_cipher_setup failed\n");
        goto EXIT;
    }

    /*
     *  The encrypted file must be structured as follows:
     *
     *        00 .. 15              Initialization Vector
     *        16 .. 31              Encrypted Block #1
     *           ..
     *      N*16 .. (N+1)*16 - 1    Encrypted Block #N
     *  (N+1)*16 .. (N+1)*16 + n    Hash(ciphertext)
     */

    if (mbedtls_cipher_get_block_size(&cipher_ctx) == 0) {
        PR_ERR("Invalid cipher block size: 0. \n");
        goto EXIT;
    }

    if (mbedtls_cipher_setkey(&cipher_ctx, input->key, mbedtls_cipher_info_get_key_bitlen(cipher_info),
                              MBEDTLS_DECRYPT) != 0) {
        PR_ERR("mbedtls_cipher_setkey() returned error\n");
        goto EXIT;
    }

    /* https://github.com/Mbed-TLS/mbedtls/issues/3665 */
    dec_tmpbuf = tal_malloc(input->data_len + tag_len);
    if (dec_tmpbuf == NULL) {
        PR_ERR("malloc dec_tmpbuf failed");
        ret = OPRT_MALLOC_FAILED;
        goto EXIT;
    }
    memcpy(dec_tmpbuf, input->data, input->data_len);
    memcpy(dec_tmpbuf + input->data_len, tag, tag_len);

    /*
     * Decrypt and write the plaintext.
     */
    ret =
        mbedtls_cipher_auth_decrypt_ext(&cipher_ctx, input->nonce, input->nonce_len, input->ad, input->ad_len,
                                        dec_tmpbuf, input->data_len + tag_len, output, input->data_len, olen, tag_len);
EXIT:
    mbedtls_cipher_free(&cipher_ctx);
    if (NULL != dec_tmpbuf) {
        tal_free(dec_tmpbuf);
        dec_tmpbuf = NULL;
    }
    return (ret);
}

int mbedtls_message_digest(mbedtls_md_type_t md_type, const uint8_t *input, size_t ilen, uint8_t *digest)
{
    if (input == NULL || ilen == 0 || digest == NULL) {
        return -1;
    }

    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    int ret = mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(md_type), 0);
    if (ret != 0) {
        PR_ERR("mbedtls_md_setup() returned -0x%04x\n", -ret);
        goto exit;
    }

    mbedtls_md_starts(&md_ctx);
    mbedtls_md_update(&md_ctx, input, ilen);
    mbedtls_md_finish(&md_ctx, digest);

exit:
    mbedtls_md_free(&md_ctx);
    return ret;
}

int mbedtls_message_digest_hmac(mbedtls_md_type_t md_type, const uint8_t *key, size_t keylen, const uint8_t *input,
                                size_t ilen, uint8_t *digest)
{
    if (key == NULL || keylen == 0 || input == NULL || ilen == 0 || digest == NULL) {
        return -1;
    }

    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    int ret = mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(md_type), 1);
    if (ret != 0) {
        PR_ERR("mbedtls_md_setup() returned -0x%04x\n", -ret);
        goto exit;
    }

    mbedtls_md_hmac_starts(&md_ctx, key, keylen);
    mbedtls_md_hmac_update(&md_ctx, input, ilen);
    mbedtls_md_hmac_finish(&md_ctx, digest);

exit:
    mbedtls_md_free(&md_ctx);
    return ret;
}

int mbedtls_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                        const uint8_t *info, size_t info_len, uint8_t *okm, size_t okm_len)
{
    const mbedtls_md_info_t *sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    unsigned char prk[32];
    unsigned char t[32];
    mbedtls_md_context_t ctx;
    size_t off = 0;
    uint8_t counter = 1;
    int ret = 0;

    if (sha256 == NULL || (salt == NULL && salt_len != 0) || (ikm == NULL && ikm_len != 0) ||
        (info == NULL && info_len != 0) || (okm == NULL && okm_len != 0) || okm_len > 255 * sizeof(t)) {
        return OPRT_INVALID_PARM;
    }

    if (okm_len == 0) {
        return OPRT_OK;
    }

#if MBEDTLS_VERSION_NUMBER >= 0x04000000
    /*
     * mbedTLS 4.x routes the legacy MD HMAC API through PSA. On ESP32-S3,
     * that path can reject valid HKDF inputs with PSA_ERROR_INVALID_ARGUMENT.
     * Use the PSA MAC API directly for 4.x and keep the mbedTLS 3.x path
     * below unchanged for platforms such as T5AI.
     */
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return (int)status;
    }

    uint8_t zero_salt[sizeof(prk)] = {0};
    const uint8_t *extract_salt = salt;
    size_t extract_salt_len = salt_len;
    if (extract_salt_len == 0) {
        extract_salt = zero_salt;
        extract_salt_len = sizeof(zero_salt);
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;
    psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
    size_t mac_len = 0;

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(extract_salt_len));
    status = psa_import_key(&attributes, extract_salt, extract_salt_len, &key_id);
    psa_reset_key_attributes(&attributes);
    if (status == PSA_SUCCESS) {
        status = psa_mac_sign_setup(&operation, key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    }
    if (status == PSA_SUCCESS && ikm_len > 0) {
        status = psa_mac_update(&operation, ikm, ikm_len);
    }
    if (status == PSA_SUCCESS) {
        status = psa_mac_sign_finish(&operation, prk, sizeof(prk), &mac_len);
    }
    psa_mac_abort(&operation);
    if (key_id != 0) {
        psa_destroy_key(key_id);
    }
    if (status != PSA_SUCCESS || mac_len != sizeof(prk)) {
        return status != PSA_SUCCESS ? (int)status : OPRT_COM_ERROR;
    }
#else
    /* extract: PRK = HMAC-SHA256(salt, IKM) */
    ret = mbedtls_md_hmac(sha256, salt, salt_len, ikm, ikm_len, prk);
    if (ret != 0) {
        return ret;
    }
#endif

    /* expand: T(i) = HMAC-SHA256(PRK, T(i-1) | info | i) */
    while (off < okm_len) {
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
        psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
        psa_key_id_t key_id = 0;
        psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
        size_t mac_len = 0;

        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
        psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(sizeof(prk)));
        status = psa_import_key(&attributes, prk, sizeof(prk), &key_id);
        psa_reset_key_attributes(&attributes);
        if (status == PSA_SUCCESS) {
            status = psa_mac_sign_setup(&operation, key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256));
        }
        if (status == PSA_SUCCESS && off > 0) {
            status = psa_mac_update(&operation, t, sizeof(t));
        }
        if (status == PSA_SUCCESS && info_len > 0) {
            status = psa_mac_update(&operation, info, info_len);
        }
        if (status == PSA_SUCCESS) {
            status = psa_mac_update(&operation, &counter, 1);
        }
        if (status == PSA_SUCCESS) {
            status = psa_mac_sign_finish(&operation, t, sizeof(t), &mac_len);
        }
        psa_mac_abort(&operation);
        if (key_id != 0) {
            psa_destroy_key(key_id);
        }
        if (status != PSA_SUCCESS || mac_len != sizeof(t)) {
            return status != PSA_SUCCESS ? (int)status : OPRT_COM_ERROR;
        }
#else
        mbedtls_md_init(&ctx);
        ret = mbedtls_md_setup(&ctx, sha256, 1);
        if (ret == 0) {
            ret = mbedtls_md_hmac_starts(&ctx, prk, sizeof(prk));
            if (ret == 0 && off > 0) {
                ret = mbedtls_md_hmac_update(&ctx, t, sizeof(t));
            }
            if (ret == 0 && info_len > 0) {
                ret = mbedtls_md_hmac_update(&ctx, info, info_len);
            }
            if (ret == 0) {
                ret = mbedtls_md_hmac_update(&ctx, &counter, 1);
            }
            if (ret == 0) {
                ret = mbedtls_md_hmac_finish(&ctx, t);
            }
        }
        mbedtls_md_free(&ctx);
        if (ret != 0) {
            return ret;
        }
#endif

        size_t n = okm_len - off;
        if (n > sizeof(t)) {
            n = sizeof(t);
        }
        memcpy(okm + off, t, n);
        off += n;
        counter++;
    }

    return 0;
}

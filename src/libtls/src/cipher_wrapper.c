// https://tls.mbed.org/module-level-design-cipher
#include "cipher_wrapper.h"
#include "tal_log.h"
#include "tal_memory.h"

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
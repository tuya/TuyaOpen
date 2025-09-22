/**
 * @file tal_symmetry.c
 * @brief This file provides the implementation of AES encryption and decryption
 * functionalities. It includes functions for creating and initializing AES
 * contexts, setting encryption and decryption keys, and performing encryption
 * and decryption operations in various modes (ECB, CBC, CTR). This
 * implementation serves as a thin layer, abstracting the underlying AES
 * functionality.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_iot_config.h"
#include "tal_symmetry.h"
#include "tal_log.h"
#include "tal_memory.h"

/**
 * @brief This function Create&initializes a aes context.
 *
 * @param[out] ctx: aes handle
 *
 * @note This API is used to create and init aes.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_aes_create_init(TKL_SYMMETRY_HANDLE *ctx)
{

    return tkl_aes_create_init(ctx);
}
/**
 * @brief This function releases and clears the specified AES context.
 *
 * @param[in] ctx: The AES context to clear.
 *
 * @note This API is used to release aes.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_aes_free(TKL_SYMMETRY_HANDLE ctx)
{
    return tkl_aes_free(ctx);
}

/**
 * @brief This function sets the encryption key.
 *
 * @param[in] ctx: The AES context to which the key should be bound.
 *                 It must be initialized.
 * @param[in] key:  The encryption key..
 * @param[in] keybits:  The size of data passed in bits. Valid options are:
 *                 <ul><li>128 bits</li>
 *                 <li>192 bits</li>
 *                 <li>256 bits</li></ul>
 *
 * @note This API is used to set aes key.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_aes_setkey_enc(TKL_SYMMETRY_HANDLE ctx, uint8_t *key, uint32_t keybits)
{
    return tkl_aes_setkey_enc(ctx, key, keybits);
}
/**
 * @brief This function sets the decryption key.
 *
 * @param[in] ctx: The AES context to which the key should be bound.
 *                 It must be initialized.
 * @param[in] key:  The decryption key..
 * @param[in] keybits:  The size of data passed in bits. Valid options are:
 *                 <ul><li>128 bits</li>
 *                 <li>192 bits</li>
 *                 <li>256 bits</li></ul>
 *
 * @note This API is used to set aes key.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_aes_setkey_dec(TKL_SYMMETRY_HANDLE ctx, uint8_t *key, uint32_t keybits)
{
    return tkl_aes_setkey_dec(ctx, key, keybits);
}

/**
 * @brief This function performs an AES encryption or decryption operation.
 *
 * @param[in] ctx:  The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * @param[in] mode     The AES operation:
 * @param[in] length   The length of the input data in Bytes. This must be a
 *                 multiple of the block size (\c 16 Bytes).
 * @param[in] input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * @param[in] output   The buffer where the output data will be written.
 *                 It must be writeable and of size \p length Bytes.
 * @note This function operates on full blocks, that is, the input size
 *         must be a multiple of the AES block size of \c 16 Bytes.
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_aes_crypt_ecb(TKL_SYMMETRY_HANDLE ctx, int32_t mode, size_t length, uint8_t *input, uint8_t *output)
{
    return tkl_aes_crypt_ecb(ctx, mode, length, input, output);
}

/**
 * @brief This function performs an AES-CBC encryption or decryption operation
 *         on full blocks.
 *
 *         It performs the operation defined in the \p mode
 *         parameter (encrypt/decrypt), on the input data buffer defined in
 *         the \p input parameter.
 *
 *         It can be called as many times as needed, until all the input
 *         data is processed. tal_aes_init(), and either
 *         tal_aes_setkey_enc() or tal_aes_setkey_dec() must be called
 *         before the first call to this API with the same context.
 *
 * @param[in] ctx:  The AES context to use for encryption or decryption.
 *                 It must be initialized and bound to a key.
 * @param[in] mode     The AES operation:
 * @param[in] length   The length of the input data in Bytes. This must be a
 *                 multiple of the block size (\c 16 Bytes).
 * @param[in] iv       Initialization vector (updated after use).
 *                 It must be a readable and writeable buffer of \c 16 Bytes.
 * @param[in] input    The buffer holding the input data.
 *                 It must be readable and of size \p length Bytes.
 * @param[in] output   The buffer where the output data will be written.
 *                 It must be writeable and of size \p length Bytes.
 *
 * @note This function operates on full blocks, that is, the input size
 *         must be a multiple of the AES block size of \c 16 Bytes.
 * @note Upon exit, the content of the IV is updated so that you can
 *         call the same function again on the next
 *         block(s) of data and get the same result as if it was
 *         encrypted in one call. This allows a "streaming" usage.
 *         If you need to retain the contents of the IV, you should
 *         either save it manually or use the cipher module instead.
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_aes_crypt_cbc(TKL_SYMMETRY_HANDLE ctx, int32_t mode, size_t length, uint8_t iv[16], uint8_t *input,
                              uint8_t *output)
{

    return tkl_aes_crypt_cbc(ctx, mode, length, iv, input, output);
}

/**
 * @brief Performs AES-CTR encryption or decryption using the provided context
 * and parameters.
 *
 * This function encrypts or decrypts the input data using the AES-CTR mode of
 * operation.
 *
 * @param ctx The handle to the AES-CTR context.
 * @param length The length of the input data.
 * @param nc_off The offset for the nonce counter.
 * @param nonce_counter The nonce counter value.
 * @param stream_block The stream block value.
 * @param input The input data to be encrypted or decrypted.
 * @param output The output buffer to store the encrypted or decrypted data.
 *
 * @return The result of the operation.
 */
OPERATE_RET tal_aes_crypt_ctr(TKL_SYMMETRY_HANDLE ctx, size_t length, size_t *nc_off, uint8_t nonce_counter[16],
                              uint8_t stream_block[16], uint8_t *input, uint8_t *output)
{
    OPERATE_RET ret;
    int32_t c, i;
    size_t n;

    if ((nc_off == NULL) || (nonce_counter == NULL) || (stream_block == NULL)) {
        ret = OPRT_INVALID_PARM;
        goto exit;
    }

    n = *nc_off;

    if (n > 0x0F) {
        ret = OPRT_INVALID_PARM;
        goto exit;
    }

    while (length--) {
        if (n == 0) {
            if ((ret = tal_aes_crypt_ecb(ctx, SYMMETRY_ENCRYPT, 16, nonce_counter, stream_block)) != OPRT_OK) {
                goto exit;
            }

            for (i = 16; i > 0; i--)
                if (++nonce_counter[i - 1] != 0) {
                    break;
                }
        }
        c = *input++;
        *output++ = (uint8_t)(c ^ stream_block[n]);

        n = (n + 1) & 0x0F;
    }

    *nc_off = n;
    ret = OPRT_OK;

exit:

    return (ret);
}

/**
 * @brief Encodes data using AES-128 ECB mode.
 *
 * This function encodes the input data using AES-128 ECB mode with the provided
 * key.
 *
 * @param data Pointer to the input data to be encoded.
 * @param len Length of the input data.
 * @param ec_data Pointer to the output encoded data.
 * @param key Pointer to the AES-128 key.
 *
 * @return OPERATE_RET Returns the status of the encoding operation.
 */
OPERATE_RET tal_aes128_ecb_encode_raw(uint8_t *data, size_t len, uint8_t *ec_data, uint8_t *key)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_enc(ctx, key, 128)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_ecb(ctx, SYMMETRY_ENCRYPT, len, data, ec_data)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

/**
 * @brief Decrypts data using AES-128 ECB mode.
 *
 * This function decrypts the input data using AES-128 ECB mode with the
 * provided key.
 *
 * @param data The input data to be decrypted.
 * @param len The length of the input data.
 * @param dec_data The buffer to store the decrypted data.
 * @param key The key used for decryption.
 *
 * @return The result of the decryption operation.
 *         - OPRT_OK if the decryption was successful.
 *         - An error code if an error occurred during decryption.
 */
OPERATE_RET tal_aes128_ecb_decode_raw(uint8_t *data, size_t len, uint8_t *dec_data, uint8_t *key)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_dec(ctx, key, 128)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_ecb(ctx, SYMMETRY_DECRYPT, len, data, dec_data)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

/**
 * @brief Encrypts data using AES-128 in CBC mode.
 *
 * This function takes the input data, key, initialization vector (IV), and
 * encrypts the data using AES-128 in CBC mode.
 *
 * @param data The input data to be encrypted.
 * @param len The length of the input data.
 * @param key The encryption key.
 * @param iv The initialization vector (IV).
 * @param ec_data The encrypted output data.
 * @return The result of the encryption operation.
 *         - OPRT_OK if the encryption was successful.
 *         - An error code if the encryption failed.
 */
OPERATE_RET tal_aes128_cbc_encode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *ec_data)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_enc(ctx, key, 128)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_cbc(ctx, SYMMETRY_ENCRYPT, len, iv, data, ec_data)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

/**
 * @brief Decrypts data using AES-128 CBC mode.
 *
 * This function decrypts the input data using the AES-128 CBC mode with the
 * provided key and initialization vector (IV).
 *
 * @param data The input data to be decrypted.
 * @param len The length of the input data.
 * @param key The AES-128 decryption key.
 * @param iv The initialization vector (IV) for CBC mode.
 * @param dec_data The buffer to store the decrypted data.
 * @return The result of the decryption operation.
 *         - OPRT_OK: Decryption was successful.
 *         - Other error codes: Decryption failed.
 */
OPERATE_RET tal_aes128_cbc_decode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *dec_data)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_dec(ctx, key, 128)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_cbc(ctx, SYMMETRY_DECRYPT, len, iv, data, dec_data)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

/**
 * @brief Encrypts data using AES-256 in CBC mode.
 *
 * This function takes the input data, key, initialization vector (IV), and
 * encrypts the data using AES-256 in CBC mode.
 *
 * @param data Pointer to the data to be encrypted.
 * @param len Length of the data to be encrypted.
 * @param key Pointer to the AES-256 key.
 * @param iv Pointer to the initialization vector (IV).
 * @param ec_data Pointer to the encrypted data output.
 *
 * @return OPERATE_RET Returns the status of the encryption operation.
 */
OPERATE_RET tal_aes256_cbc_encode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *ec_data)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_enc(ctx, key, 256)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_cbc(ctx, SYMMETRY_ENCRYPT, len, iv, data, ec_data)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

/**
 * @brief Decrypts data using AES-256 in CBC mode.
 *
 * This function takes the input data, key, initialization vector (IV), and
 * decrypts the data using AES-256 in CBC mode.
 *
 * @param data Pointer to the input data to be decrypted.
 * @param len Length of the input data.
 * @param key Pointer to the AES-256 key.
 * @param iv Pointer to the initialization vector (IV).
 * @param dec_data Pointer to the decrypted data.
 * @return OPERATE_RET Returns the status of the decryption operation.
 */
OPERATE_RET tal_aes256_cbc_decode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *dec_data)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_dec(ctx, key, 256)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_cbc(ctx, SYMMETRY_DECRYPT, len, iv, data, dec_data)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

/**
 * @brief Performs AES-256 CTR mode encryption/decryption on the input data.
 *
 * This function takes the input data, key, nonce counter, and stream block as
 * parameters and performs AES-256 CTR mode encryption/decryption on the input
 * data. The result is stored in the output buffer.
 *
 * @param input The input data to be encrypted/decrypted.
 * @param len The length of the input data.
 * @param key The AES-256 key used for encryption/decryption.
 * @param nc_off The offset of the nonce counter.
 * @param nonce_counter The nonce counter used for encryption/decryption.
 * @param stream_block The stream block used for encryption/decryption.
 * @param output The output buffer to store the encrypted/decrypted data.
 * @return The result of the operation.
 */
OPERATE_RET tal_aes256_ctr_raw(uint8_t *input, size_t len, uint8_t *key, size_t *nc_off, uint8_t nonce_counter[16],
                               uint8_t stream_block[16], uint8_t *output)
{
    OPERATE_RET ret;
    TKL_SYMMETRY_HANDLE ctx = NULL;

    if ((ret = tal_aes_create_init(&ctx)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_setkey_enc(ctx, key, 256)) != OPRT_OK) {
        goto exit;
    }

    if ((ret = tal_aes_crypt_ctr(ctx, len, nc_off, nonce_counter, stream_block, input, output)) != OPRT_OK) {
        goto exit;
    }

exit:
    if (ctx) {
        ret = tal_aes_free(ctx);
    }

    return (ret);
}

static void add_pkcs_padding(uint8_t *output, size_t output_len, size_t data_len)
{
    size_t padding_len = output_len - data_len;
    uint8_t i;

    // PKCS7 (and PKCS5) padding: fill with ll bytes, with ll = padding_len
    for (i = 0; i < padding_len; i++) {
        output[data_len + i] = (uint8_t)padding_len;
    }
}

static uint32_t __Add_Pkcs(uint8_t *p, uint32_t len)
{
    // The tail is filled with pkcs7, returning the length after filling
    int cz = 16 - len % 16;

    add_pkcs_padding(p, len + cz, len);

    return (len + cz);
}

/**
 * @brief Performs PKCS7 padding on a buffer.
 *
 * This function applies PKCS7 padding to the given buffer to ensure that the
 * length of the buffer is a multiple of 16 bytes.
 *
 * @param p_buffer Pointer to the buffer to be padded.
 * @param length   Length of the buffer.
 *
 * @return The new length of the buffer after padding.
 */
uint32_t tal_pkcs7padding_buffer(uint8_t *p_buffer, uint32_t length)
{
    return __Add_Pkcs(p_buffer, length);
}

/**
 * @brief Calculates the actual length of decrypted data after AES decryption.
 *
 * This function takes in the decrypted data and its length as input and
 * calculates the actual length of the decrypted data after AES decryption.
 *
 * @param dec_data Pointer to the decrypted data.
 * @param dec_data_len Length of the decrypted data.
 * @return The actual length of the decrypted data after AES decryption.
 */
int32_t tal_aes_get_actual_length(uint8_t *dec_data, uint32_t dec_data_len)
{
    if ((NULL == dec_data) || (0 == dec_data_len) || (dec_data_len % 16 != 0)) {
        return OPRT_INVALID_PARM;
    }

    uint8_t lastdata_val = dec_data[dec_data_len - 1];
    if (lastdata_val > 16) {
        return -2;
    }

    if (dec_data_len <= lastdata_val) {
        return -3;
    }

    return dec_data_len - lastdata_val;
}

/**
 * @brief Encodes data using AES-128 ECB mode.
 *
 * This function encodes the input data using AES-128 ECB mode with the provided
 * key.
 *
 * @param data Pointer to the input data to be encoded.
 * @param len Length of the input data.
 * @param ec_data Pointer to the output encoded data.
 * @param key Pointer to the AES-128 key.
 *
 * @return OPERATE_RET Returns the status of the encoding operation.
 */
OPERATE_RET tal_aes128_ecb_encode(uint8_t *data, uint32_t len, uint8_t **ec_data, uint32_t *ec_len, uint8_t *key)
{
    if (NULL == data || NULL == key || NULL == ec_data || NULL == ec_len || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    *ec_data = tal_malloc(len + 16);
    if (NULL == *ec_data) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(*ec_data, data, len);
    uint32_t pkcs7Len = __Add_Pkcs(*ec_data, len);
    *ec_len = pkcs7Len;

    OPERATE_RET ret = tal_aes128_ecb_encode_raw(*ec_data, pkcs7Len, *ec_data, key);
    if (ret != OPRT_OK) {
        tal_free(*ec_data);
        *ec_data = NULL;
        return ret;
    }
    return OPRT_OK;
}

/**
 * @brief Decrypts data using AES-128 ECB mode.
 *
 * This function decrypts the input data using AES-128 ECB mode with the
 * provided key.
 *
 * @param data The input data to be decrypted.
 * @param len The length of the input data.
 * @param dec_data The buffer to store the decrypted data.
 * @param key The encryption key used for decryption.
 *
 * @return The result of the decryption operation.
 *         - OPRT_OK: Decryption was successful.
 *         - Other error codes: Decryption failed.
 */
OPERATE_RET tal_aes128_ecb_decode(uint8_t *data, uint32_t len, uint8_t **dec_data, uint32_t *dec_len, uint8_t *key)
{
    if (NULL == data || 0 == len || NULL == key || NULL == dec_data || NULL == dec_len) {
        return OPRT_INVALID_PARM;
    }

    *dec_len = len;
    *dec_data = (uint8_t *)tal_malloc(len + 1);
    if (NULL == *dec_data) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(*dec_data, data, len);
    (*dec_data)[len] = 0;
    OPERATE_RET ret = tal_aes128_ecb_decode_raw(*dec_data, len, *dec_data, key);
    if (ret != OPRT_OK) {
        tal_free(*dec_data);
        *dec_data = NULL;
        return ret;
    }
    (*dec_data)[len] = 0;
    return OPRT_OK;
}

/**
 * @brief Encrypts data using AES-128 in CBC mode.
 *
 * This function takes the input data, key, initialization vector (IV), and
 * encrypts the data using AES-128 in CBC mode.
 *
 * @param data The input data to be encrypted.
 * @param len The length of the input data.
 * @param key The encryption key.
 * @param iv The initialization vector (IV).
 * @param ec_data The encrypted data output.
 * @return The result of the encryption operation.
 */
OPERATE_RET tal_aes128_cbc_encode(uint8_t *data, uint32_t len, uint8_t *key, uint8_t *iv, uint8_t **ec_data,
                                  uint32_t *ec_len)
{
    if (NULL == data || 0 == len || NULL == key || NULL == iv || NULL == ec_data || NULL == ec_len) {
        return OPRT_INVALID_PARM;
    }

    *ec_data = tal_malloc(len + 16);
    if (NULL == *ec_data) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(*ec_data, data, len);
    uint32_t pkcs7Len = __Add_Pkcs(*ec_data, len);
    *ec_len = pkcs7Len;

    OPERATE_RET ret = tal_aes128_cbc_encode_raw(*ec_data, pkcs7Len, key, iv, *ec_data);
    if (ret != OPRT_OK) {
        tal_free(*ec_data);
        *ec_data = NULL;
        return ret;
    }
    return OPRT_OK;
}

/**
 * @brief Decrypts data using AES-128 CBC mode.
 *
 * This function decrypts the input data using AES-128 CBC mode with the
 * provided key and initialization vector (IV).
 *
 * @param data The input data to be decrypted.
 * @param len The length of the input data.
 * @param key The AES-128 encryption key.
 * @param iv The initialization vector (IV) for CBC mode.
 * @param dec_data The buffer to store the decrypted data.
 *
 * @return The result of the decryption operation.
 *         - OPRT_OK: Decryption was successful.
 *         - Other error codes: Decryption failed.
 */
OPERATE_RET tal_aes128_cbc_decode(uint8_t *data, uint32_t len, uint8_t *key, uint8_t *iv, uint8_t **dec_data,
                                  uint32_t *dec_len)
{
    if (NULL == data || 0 == len || NULL == key || NULL == iv || NULL == dec_data || NULL == dec_len) {
        return OPRT_INVALID_PARM;
    }

    *dec_len = len;
    *dec_data = (uint8_t *)tal_malloc(len + 1);
    if (NULL == *dec_data) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(*dec_data, data, len);
    (*dec_data)[len] = 0;
    OPERATE_RET ret = tal_aes128_cbc_decode_raw(*dec_data, len, key, iv, *dec_data);
    if (ret != OPRT_OK) {
        tal_free(*dec_data);
        *dec_data = NULL;
        return ret;
    }
    (*dec_data)[len] = 0;
    return OPRT_OK;
}

/**
 * @brief Frees the memory allocated for AES data.
 *
 * This function frees the memory allocated for AES data.
 *
 * @param data Pointer to the AES data to be freed.
 * @return OPERATE_RET Returns OPERATE_RET_OK if the memory is successfully
 * freed, otherwise returns an error code.
 */
OPERATE_RET tal_aes_free_data(uint8_t *data)
{
    if (data == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_free(data);
    return OPRT_OK;
}

#if defined(ENABLE_TAL_SECURITY_SELF_TEST)
/*
 * AES test vectors from:
 *
 * http://csrc.nist.gov/archive/aes/rijndael/rijndael-vals.zip
 */
static const uint8_t aes_test_ecb_dec[3][16] = {
    {0x44, 0x41, 0x6A, 0xC2, 0xD1, 0xF5, 0x3C, 0x58, 0x33, 0x03, 0x91, 0x7E, 0x6B, 0xE9, 0xEB, 0xE0},
    {0x48, 0xE3, 0x1E, 0x9E, 0x25, 0x67, 0x18, 0xF2, 0x92, 0x29, 0x31, 0x9C, 0x19, 0xF1, 0x5B, 0xA4},
    {0x05, 0x8C, 0xCF, 0xFD, 0xBB, 0xCB, 0x38, 0x2D, 0x1F, 0x6F, 0x56, 0x58, 0x5D, 0x8A, 0x4A, 0xDE}};

static const uint8_t aes_test_ecb_enc[3][16] = {
    {0xC3, 0x4C, 0x05, 0x2C, 0xC0, 0xDA, 0x8D, 0x73, 0x45, 0x1A, 0xFE, 0x5F, 0x03, 0xBE, 0x29, 0x7F},
    {0xF3, 0xF6, 0x75, 0x2A, 0xE8, 0xD7, 0x83, 0x11, 0x38, 0xF0, 0x41, 0x56, 0x06, 0x31, 0xB1, 0x14},
    {0x8B, 0x79, 0xEE, 0xCC, 0x93, 0xA0, 0xEE, 0x5D, 0xFF, 0x30, 0xB4, 0xEA, 0x21, 0x63, 0x6D, 0xA4}};

static const uint8_t aes_test_cbc_dec[3][16] = {
    {0xFA, 0xCA, 0x37, 0xE0, 0xB0, 0xC8, 0x53, 0x73, 0xDF, 0x70, 0x6E, 0x73, 0xF7, 0xC9, 0xAF, 0x86},
    {0x5D, 0xF6, 0x78, 0xDD, 0x17, 0xBA, 0x4E, 0x75, 0xB6, 0x17, 0x68, 0xC6, 0xAD, 0xEF, 0x7C, 0x7B},
    {0x48, 0x04, 0xE1, 0x81, 0x8F, 0xE6, 0x29, 0x75, 0x19, 0xA3, 0xE8, 0x8C, 0x57, 0x31, 0x04, 0x13}};

static const uint8_t aes_test_cbc_enc[3][16] = {
    {0x8A, 0x05, 0xFC, 0x5E, 0x09, 0x5A, 0xF4, 0x84, 0x8A, 0x08, 0xD3, 0x28, 0xD3, 0x68, 0x8E, 0x3D},
    {0x7B, 0xD9, 0x66, 0xD5, 0x3A, 0xD8, 0xC1, 0xBB, 0x85, 0xD2, 0xAD, 0xFA, 0xE8, 0x7B, 0xB1, 0x04},
    {0xFE, 0x3C, 0x53, 0x65, 0x3E, 0x2F, 0x45, 0xB5, 0x6F, 0xCD, 0x88, 0xB2, 0xCC, 0x89, 0x8F, 0xF0}};

/*
 * AES-CTR test vectors from:
 *
 * http://www.faqs.org/rfcs/rfc3686.html
 */

static const uint8_t aes_test_ctr_key[3][16] = {
    {0xAE, 0x68, 0x52, 0xF8, 0x12, 0x10, 0x67, 0xCC, 0x4B, 0xF7, 0xA5, 0x76, 0x55, 0x77, 0xF3, 0x9E},
    {0x7E, 0x24, 0x06, 0x78, 0x17, 0xFA, 0xE0, 0xD7, 0x43, 0xD6, 0xCE, 0x1F, 0x32, 0x53, 0x91, 0x63},
    {0x76, 0x91, 0xBE, 0x03, 0x5E, 0x50, 0x20, 0xA8, 0xAC, 0x6E, 0x61, 0x85, 0x29, 0xF9, 0xA0, 0xDC}};

static const uint8_t aes_test_ctr_nonce_counter[3][16] = {
    {0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
    {0x00, 0x6C, 0xB6, 0xDB, 0xC0, 0x54, 0x3B, 0x59, 0xDA, 0x48, 0xD9, 0x0B, 0x00, 0x00, 0x00, 0x01},
    {0x00, 0xE0, 0x01, 0x7B, 0x27, 0x77, 0x7F, 0x3F, 0x4A, 0x17, 0x86, 0xF0, 0x00, 0x00, 0x00, 0x01}};

static const uint8_t aes_test_ctr_pt[3][48] = {
    {0x53, 0x69, 0x6E, 0x67, 0x6C, 0x65, 0x20, 0x62, 0x6C, 0x6F, 0x63, 0x6B, 0x20, 0x6D, 0x73, 0x67},

    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F},

    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
     0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23}};

static const uint8_t aes_test_ctr_ct[3][48] = {
    {0xE4, 0x09, 0x5D, 0x4F, 0xB7, 0xA7, 0xB3, 0x79, 0x2D, 0x61, 0x75, 0xA3, 0x26, 0x13, 0x11, 0xB8},
    {0x51, 0x04, 0xA1, 0x06, 0x16, 0x8A, 0x72, 0xD9, 0x79, 0x0D, 0x41, 0xEE, 0x8E, 0xDA, 0xD3, 0x88,
     0xEB, 0x2E, 0x1E, 0xFC, 0x46, 0xDA, 0x57, 0xC8, 0xFC, 0xE6, 0x30, 0xDF, 0x91, 0x41, 0xBE, 0x28},
    {0xC1, 0xCF, 0x48, 0xA8, 0x9F, 0x2F, 0xFD, 0xD9, 0xCF, 0x46, 0x52, 0xE9, 0xEF, 0xDB, 0x72, 0xD7, 0x45, 0x40,
     0xA4, 0x2B, 0xDE, 0x6D, 0x78, 0x36, 0xD5, 0x9A, 0x5C, 0xEA, 0xAE, 0xF3, 0x10, 0x53, 0x25, 0xB2, 0x07, 0x2F}};

static const int aes_test_ctr_len[3] = {16, 32, 36};

/*
 * Checkup routine
 */
OPERATE_RET tal_aes_self_test(int32_t verbose)
{
    OPERATE_RET ret;
    int32_t i, j, u, mode;
    uint32_t keybits;
    uint8_t key[32];
    uint8_t buf[64];
    const uint8_t *aes_tests;
    uint8_t iv[16];
    uint8_t prv[16];
    TKL_SYMMETRY_HANDLE ctx;
    size_t offset;
    int32_t len;
    uint8_t nonce_counter[16];
    uint8_t stream_block[16];

    memset(key, 0, 32);
    tal_aes_create_init(&ctx);

    /*
     * ECB mode
     */
    for (i = 0; i < 6; i++) {
        u = i >> 1;
        keybits = 128 + u * 64;
        mode = i & 1;

        if (verbose != 0)
            PR_DEBUG("  AES-ECB-%3d (%s): ", keybits, (mode == SYMMETRY_DECRYPT) ? "dec" : "enc");

        memset(buf, 0, 16);

        if (mode == SYMMETRY_DECRYPT) {
            ret = tal_aes_setkey_dec(ctx, key, keybits);
            aes_tests = aes_test_ecb_dec[u];
        } else {
            ret = tal_aes_setkey_enc(ctx, key, keybits);
            aes_tests = aes_test_ecb_enc[u];
        }

        /*
         * AES-192 is an optional feature that may be unavailable when
         * there is an alternative underlying implementation i.e. when
         * MBEDTLS_AES_ALT is defined.
         */
        if (keybits == 192) {
            PR_DEBUG("skipped\n");
            continue;
        } else if (ret != 0) {
            goto exit;
        }

        for (j = 0; j < 10000; j++) {
            ret = tal_aes_crypt_ecb(ctx, mode, 16, buf, buf);
            if (ret != 0) {
                goto exit;
            }
        }

        if (memcmp(buf, aes_tests, 16) != 0) {
            ret = 1;
            goto exit;
        }

        if (verbose != 0) {
            PR_DEBUG("passed\n");
        }
    }

    if (verbose != 0) {
        PR_DEBUG("\n");
    }

    /*
     * CBC mode
     */
    for (i = 0; i < 6; i++) {
        u = i >> 1;
        keybits = 128 + u * 64;
        mode = i & 1;

        if (verbose != 0)
            PR_DEBUG("  AES-CBC-%3d (%s): ", keybits, (mode == SYMMETRY_DECRYPT) ? "dec" : "enc");

        memset(iv, 0, 16);
        memset(prv, 0, 16);
        memset(buf, 0, 16);

        if (mode == SYMMETRY_DECRYPT) {
            ret = tal_aes_setkey_dec(ctx, key, keybits);
            aes_tests = aes_test_cbc_dec[u];
        } else {
            ret = tal_aes_setkey_enc(ctx, key, keybits);
            aes_tests = aes_test_cbc_enc[u];
        }

        /*
         * AES-192 is an optional feature that may be unavailable when
         * there is an alternative underlying implementation i.e. when
         * MBEDTLS_AES_ALT is defined.
         */
        if (keybits == 192) {
            PR_DEBUG("skipped\n");
            continue;
        } else if (ret != 0) {
            goto exit;
        }

        for (j = 0; j < 10000; j++) {
            if (mode == SYMMETRY_ENCRYPT) {
                uint8_t tmp[16];

                memcpy(tmp, prv, 16);
                memcpy(prv, buf, 16);
                memcpy(buf, tmp, 16);
            }

            ret = tal_aes_crypt_cbc(ctx, mode, 16, iv, buf, buf);
            if (ret != 0) {
                goto exit;
            }
        }

        if (memcmp(buf, aes_tests, 16) != 0) {
            ret = 1;
            goto exit;
        }

        if (verbose != 0) {
            PR_DEBUG("passed\n");
        }
    }

    if (verbose != 0) {
        PR_DEBUG("\n");
    }

    /*
     * CTR mode
     */
    for (i = 0; i < 6; i++) {
        u = i >> 1;
        mode = i & 1;

        if (verbose != 0)
            PR_DEBUG("  AES-CTR-128 (%s): ", (mode == SYMMETRY_DECRYPT) ? "dec" : "enc");

        memcpy(nonce_counter, aes_test_ctr_nonce_counter[u], 16);
        memcpy(key, aes_test_ctr_key[u], 16);

        offset = 0;
        if ((ret = tal_aes_setkey_enc(ctx, key, 128)) != 0) {
            goto exit;
        }

        len = aes_test_ctr_len[u];

        if (mode == SYMMETRY_DECRYPT) {
            memcpy(buf, aes_test_ctr_ct[u], len);
            aes_tests = aes_test_ctr_pt[u];
        } else {
            memcpy(buf, aes_test_ctr_pt[u], len);
            aes_tests = aes_test_ctr_ct[u];
        }

        ret = tal_aes_crypt_ctr(ctx, len, &offset, nonce_counter, stream_block, buf, buf);
        if (ret != 0) {
            goto exit;
        }

        if (memcmp(buf, aes_tests, len) != 0) {
            ret = 1;
            goto exit;
        }

        if (verbose != 0) {
            PR_DEBUG("passed\n");
        }
    }

    if (verbose != 0) {
        PR_DEBUG("\n");
    }

    ret = 0;

exit:
    if (ret != 0 && verbose != 0) {
        PR_DEBUG("failed\n");
    }

    tal_aes_free(ctx);

    return (ret);
}

#endif
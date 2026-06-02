/**
 * @file mbedtls_symmetry.c
 * @brief Provides symmetric encryption and decryption functionalities using mbedtls.
 *        This includes operations like AES encryption and decryption in various modes
 *        (e.g., ECB, CBC).
 * 
 * This file implements the symmetric encryption and decryption functionalities provided
 * by the mbedtls library. It wraps the mbedtls AES functionalities into a more simplified
 * interface for ease of use within applications requiring encryption and decryption
 * capabilities. The provided functions include initialization, setting encryption and
 * decryption keys, performing encryption and decryption operations, and cleanup.
 * 
 * Supported operations:
 * - AES encryption and decryption in ECB mode.
 * - AES encryption and decryption in CBC mode.
 * - Key initialization for both encryption and decryption.
 * - Context initialization and free operations.
 * 
 * Usage of this file should be preceded by proper initialization of the AES context
 * and followed by the appropriate cleanup to prevent memory leaks.
 * 
 * @note This file is part of the Tuya IoT SDK and is subject to the SDK's license terms.
 * 
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 * 
 */

#include "tuya_iot_config.h"
#include "tkl_memory.h"
#include "tkl_symmetry.h"
#if !defined(ENABLE_PLATFORM_AES)
#include "cipher_wrapper.h"
#include "mbedtls/aes.h"
#endif

#if !defined(ENABLE_PLATFORM_AES)

/**
* @brief This function Create&initializes a aes context.
*
* @param[out] ctx: aes handle
*
* @note This API is used to create and init aes.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET  tkl_aes_create_init( TKL_SYMMETRY_HANDLE *ctx ){

    mbedtls_aes_context *mbedtls_aes_ctx;

    mbedtls_aes_ctx =(mbedtls_aes_context *) tkl_system_malloc(sizeof(mbedtls_aes_context));    
    mbedtls_aes_init(mbedtls_aes_ctx);
    *ctx = mbedtls_aes_ctx;

    return OPRT_OK;
}
/**
* @brief This function releases and clears the specified AES context.
*
* @param[in] ctx: The AES context to clear.
*
* @note This API is used to release aes.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET  tkl_aes_free( TKL_SYMMETRY_HANDLE ctx )
{
    if(ctx != NULL){
        mbedtls_aes_free( (mbedtls_aes_context *) ctx );
        tkl_system_free(ctx);
    }

    return OPRT_OK;
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
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET  tkl_aes_setkey_enc( TKL_SYMMETRY_HANDLE ctx, const uint8_t *key,
                    uint32_t keybits )
{
    if( mbedtls_aes_setkey_enc( (mbedtls_aes_context *)ctx, key,keybits ) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
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
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET  tkl_aes_setkey_dec( TKL_SYMMETRY_HANDLE ctx, const uint8_t *key,
                    uint32_t keybits )
{
    if( mbedtls_aes_setkey_dec( (mbedtls_aes_context *)ctx, key,keybits ) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
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
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET  tkl_aes_crypt_ecb( TKL_SYMMETRY_HANDLE ctx,
                    int32_t mode,
                    size_t length,
                    const uint8_t *input,
                    uint8_t *output )
{
    uint32_t i;

    if( (length % 16) != 0)
        return OPRT_INVALID_PARM;

    for(i = 0;i < length;i += 16){
        if( mbedtls_aes_crypt_ecb( (mbedtls_aes_context *)ctx,mode,input + i,output + i) != 0)
            return OPRT_COM_ERROR;
    }

    return OPRT_OK;
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
*         data is processed. tkl_aes_init(), and either
*         tkl_aes_setkey_enc() or tkl_aes_setkey_dec() must be called
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
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET  tkl_aes_crypt_cbc( TKL_SYMMETRY_HANDLE ctx,
                    int32_t mode,
                    size_t length,
                    uint8_t iv[16],
                    const uint8_t *input,
                    uint8_t *output )
{
    if( mbedtls_aes_crypt_cbc( (mbedtls_aes_context *)ctx,mode,length,iv,input,output ) != 0)
        return OPRT_COM_ERROR;

    return OPRT_OK;
}

/**
* @brief This function performs AES-GCM authenticated encryption.
*
* @param[in]  key:        The encryption key.
* @param[in]  key_len:    The length of the key in bytes. Valid options are:
*                         <ul><li>16 bytes (128 bits)</li>
*                         <li>24 bytes (192 bits)</li>
*                         <li>32 bytes (256 bits)</li></ul>
* @param[in]  nonce:      The initialization vector (nonce).
* @param[in]  nonce_len:  The length of the nonce in bytes.
* @param[in]  ad:         The additional authenticated data (AAD). May be NULL if ad_len is 0.
* @param[in]  ad_len:     The length of the AAD in bytes.
* @param[in]  input:      The buffer holding the plaintext input data.
* @param[in]  input_len:  The length of the input data in bytes.
* @param[out] output:     The buffer where the ciphertext will be written.
*                         Must be at least input_len bytes.
* @param[out] output_len: The number of bytes written to the output buffer.
* @param[out] tag:        The buffer where the authentication tag will be written.
* @param[in]  tag_len:    The length of the authentication tag in bytes.
*
* @note This API is used to encrypt and authenticate data using AES-GCM.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tkl_aes_gcm_encode(const uint8_t *key, uint32_t key_len,
                                const uint8_t *nonce, uint32_t nonce_len,
                                const uint8_t *ad, uint32_t ad_len,
                                const uint8_t *input, uint32_t input_len,
                                uint8_t *output, uint32_t *output_len,
                                uint8_t *tag, uint32_t tag_len)
{
    if (key == NULL || input == NULL || output == NULL || output_len == NULL || tag == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (key_len != 16 && key_len != 24 && key_len != 32) {
        return OPRT_INVALID_PARM;
    }

    cipher_params_t params = {0};
    params.key         = (unsigned char *)key;
    params.key_len     = key_len;
    params.nonce       = (unsigned char *)nonce;
    params.nonce_len   = nonce_len;
    params.ad          = (unsigned char *)ad;
    params.ad_len      = ad_len;
    params.data        = (unsigned char *)input;
    params.data_len    = input_len;
    params.cipher_type = (mbedtls_cipher_type_t)(MBEDTLS_CIPHER_AES_128_GCM + ((key_len / 8) - 2));

    size_t olen = 0;
    int ret = mbedtls_cipher_auth_encrypt_wrapper(&params, output, &olen, tag, tag_len);
    if (ret != 0) {
        return ret;
    }

    *output_len = (uint32_t)olen;

    return OPRT_OK;
}

/**
* @brief This function performs AES-GCM authenticated decryption.
*
* @param[in]  key:        The decryption key.
* @param[in]  key_len:    The length of the key in bytes. Valid options are:
*                         <ul><li>16 bytes (128 bits)</li>
*                         <li>24 bytes (192 bits)</li>
*                         <li>32 bytes (256 bits)</li></ul>
* @param[in]  nonce:      The initialization vector (nonce) used during encryption.
* @param[in]  nonce_len:  The length of the nonce in bytes.
* @param[in]  ad:         The additional authenticated data (AAD). May be NULL if ad_len is 0.
* @param[in]  ad_len:     The length of the AAD in bytes.
* @param[in]  input:      The buffer holding the ciphertext input data.
* @param[in]  input_len:  The length of the input data in bytes.
* @param[out] output:     The buffer where the decrypted plaintext will be written.
*                         Must be at least input_len bytes.
* @param[out] output_len: The number of bytes written to the output buffer.
* @param[in]  tag:        The authentication tag to verify.
* @param[in]  tag_len:    The length of the authentication tag in bytes.
*
* @note This API is used to decrypt and verify data using AES-GCM.
*       Returns an error if tag verification fails.
*
* @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
*/
OPERATE_RET tkl_aes_gcm_decode(const uint8_t *key, uint32_t key_len,
                                const uint8_t *nonce, uint32_t nonce_len,
                                const uint8_t *ad, uint32_t ad_len,
                                const uint8_t *input, uint32_t input_len,
                                uint8_t *output, uint32_t *output_len,
                                uint8_t *tag, uint32_t tag_len)
{
    if (key == NULL || input == NULL || output == NULL || output_len == NULL || tag == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (key_len != 16 && key_len != 24 && key_len != 32) {
        return OPRT_INVALID_PARM;
    }

    cipher_params_t params = {0};
    params.key         = (unsigned char *)key;
    params.key_len     = key_len;
    params.nonce       = (unsigned char *)nonce;
    params.nonce_len   = nonce_len;
    params.ad          = (unsigned char *)ad;
    params.ad_len      = ad_len;
    params.data        = (unsigned char *)input;
    params.data_len    = input_len;
    params.cipher_type = (mbedtls_cipher_type_t)(MBEDTLS_CIPHER_AES_128_GCM + ((key_len / 8) - 2));

    size_t olen = 0;
    int ret = mbedtls_cipher_auth_decrypt_wrapper(&params, output, &olen, tag, tag_len);
    if (ret != 0) {
        return ret;
    }
    
    *output_len = (uint32_t)olen;

return OPRT_OK;
}

#endif
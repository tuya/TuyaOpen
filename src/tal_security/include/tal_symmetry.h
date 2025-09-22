/**
 * @file tal_symmetry.h
 * @brief Symmetric cryptography utilities for Tuya SDK.
 *
 * This header file defines the interfaces for symmetric cryptography operations
 * within the Tuya SDK, including AES encryption and decryption. It provides a
 * set of functions for creating, initializing, and managing AES contexts, as
 * well as performing encryption and decryption operations using these contexts.
 * The file abstracts the complexity of cryptographic operations, offering a
 * simplified API for developers to incorporate symmetric encryption into their
 * Tuya-based applications.
 *
 * The symmetric cryptography utilities are essential for securing communication
 * and data at rest, providing confidentiality and integrity through encryption.
 * By leveraging these utilities, developers can ensure that sensitive
 * information is protected against unauthorized access and manipulation.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It requires the underlying cryptographic
 * library "tkl_symmetry.h" for actual cryptographic operations.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_SYMMETRY_H__
#define __TAL_SYMMETRY_H__

#include "tkl_symmetry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYMMETRY_DECRYPT = 0,
    SYMMETRY_ENCRYPT = 1,
} TAL_SYMMETRY_CRYPT_MODE;

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
OPERATE_RET tal_aes_create_init(TKL_SYMMETRY_HANDLE *ctx);

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
OPERATE_RET tal_aes_free(TKL_SYMMETRY_HANDLE ctx);

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
OPERATE_RET tal_aes_setkey_enc(TKL_SYMMETRY_HANDLE ctx, uint8_t *key, uint32_t keybits);

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
OPERATE_RET tal_aes_setkey_dec(TKL_SYMMETRY_HANDLE ctx, uint8_t *key, uint32_t keybits);

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
OPERATE_RET tal_aes_crypt_ecb(TKL_SYMMETRY_HANDLE ctx, int32_t mode, size_t length, uint8_t *input, uint8_t *output);

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
                              uint8_t *output);

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
                              uint8_t stream_block[16], uint8_t *input, uint8_t *output);

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
OPERATE_RET tal_aes128_ecb_encode_raw(uint8_t *data, size_t len, uint8_t *ec_data, uint8_t *key);

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
OPERATE_RET tal_aes128_ecb_decode_raw(uint8_t *data, size_t len, uint8_t *dec_data, uint8_t *key);

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
OPERATE_RET tal_aes128_cbc_encode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *ec_data);

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
OPERATE_RET tal_aes128_cbc_decode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *dec_data);

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
OPERATE_RET tal_aes256_cbc_encode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *ec_data);

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
OPERATE_RET tal_aes256_cbc_decode_raw(uint8_t *data, size_t len, uint8_t *key, uint8_t *iv, uint8_t *dec_data);

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
                               uint8_t stream_block[16], uint8_t *output);

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
uint32_t tal_pkcs7padding_buffer(uint8_t *p_buffer, uint32_t length);

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
int32_t tal_aes_get_actual_length(uint8_t *dec_data, uint32_t dec_data_len);

/**
 * @brief Encrypts data using AES-128 ECB mode.
 *
 * This function takes the input data and encrypts it using AES-128 ECB mode.
 * The encrypted data is stored in the `ec_data` buffer, and the length of the
 * encrypted data is stored in the `ec_len` variable.
 *
 * @param data The input data to be encrypted.
 * @param len The length of the input data.
 * @param ec_data Pointer to the buffer where the encrypted data will be stored.
 * @param ec_len Pointer to the variable where the length of the encrypted data
 * will be stored.
 * @param key The encryption key.
 * @return OPERATE_RET Returns the status of the encryption operation.
 */
OPERATE_RET tal_aes128_ecb_encode(uint8_t *data, uint32_t len, uint8_t **ec_data, uint32_t *ec_len, uint8_t *key);

/**
 * @brief Decrypts data using AES-128 ECB mode.
 *
 * This function decrypts the input data using AES-128 ECB mode with the
 * provided key.
 *
 * @param data The input data to be decrypted.
 * @param len The length of the input data.
 * @param dec_data Pointer to the decrypted data. The memory for the decrypted
 * data will be allocated by the function and should be freed by the caller.
 * @param dec_len Pointer to the length of the decrypted data. The function will
 * update this value with the actual length of the decrypted data.
 * @param key The encryption key used for decryption.
 *
 * @return OPERATE_RET Returns OPRT_OK if the decryption is successful, or an
 * error code if an error occurs.
 */
OPERATE_RET tal_aes128_ecb_decode(uint8_t *data, uint32_t len, uint8_t **dec_data, uint32_t *dec_len, uint8_t *key);

/**
 * @brief Performs AES-128 CBC encryption on the input data using the provided
 * key and initialization vector (IV).
 *
 * This function takes the input data, key, IV, and performs AES-128 CBC
 * encryption on the data. The encrypted data is returned in the `ec_data`
 * parameter, and the length of the encrypted data is returned in the `ec_len`
 * parameter.
 *
 * @param data The input data to be encrypted.
 * @param len The length of the input data.
 * @param key The encryption key.
 * @param iv The initialization vector (IV).
 * @param ec_data The output parameter to store the encrypted data.
 * @param ec_len The output parameter to store the length of the encrypted data.
 * @return The operation result. Possible return values are defined by the
 * `OPERATE_RET` enum.
 */
OPERATE_RET tal_aes128_cbc_encode(uint8_t *data, uint32_t len, uint8_t *key, uint8_t *iv, uint8_t **ec_data,
                                  uint32_t *ec_len);

/**
 * @brief Decrypts data using AES-128 in CBC mode.
 *
 * This function takes the input `data` of length `len` and decrypts it using
 * the AES-128 algorithm in CBC mode. The decryption is performed using the
 * provided `key` and `iv` (initialization vector). The decrypted data is stored
 * in the `dec_data` buffer, and the length of the decrypted data is stored in
 * `dec_len`.
 *
 * @param data The input data to be decrypted.
 * @param len The length of the input data.
 * @param key The encryption key used for decryption.
 * @param iv The initialization vector used for decryption.
 * @param dec_data The buffer to store the decrypted data.
 * @param dec_len The length of the decrypted data.
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - Other error codes indicating failure.
 */
OPERATE_RET tal_aes128_cbc_decode(uint8_t *data, uint32_t len, uint8_t *key, uint8_t *iv, uint8_t **dec_data,
                                  uint32_t *dec_len);

/**
 * @brief Frees the memory allocated for AES data.
 *
 * This function frees the memory allocated for AES data.
 *
 * @param data Pointer to the AES data to be freed.
 * @return OPERATE_RET Returns OPERATE_RET_OK if the memory is successfully
 * freed, otherwise returns an error code.
 */
OPERATE_RET tal_aes_free_data(uint8_t *data);

/**
 * @brief Performs a self-test for the AES encryption algorithm.
 *
 * This function tests the correctness of the AES encryption algorithm
 * implementation. It returns an error code indicating the success or failure of
 * the self-test.
 *
 * @param verbose A flag indicating whether to print verbose output during the
 * self-test. Set to a non-zero value to enable verbose output, or 0 to disable
 * it.
 *
 * @return An error code indicating the success or failure of the self-test.
 *         Returns OPRT_OK if the self-test passed successfully, or an error
 * code if it failed.
 */
OPERATE_RET tal_aes_self_test(int32_t verbose);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

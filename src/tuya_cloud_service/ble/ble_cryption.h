/**
 * @file ble_cryption.h
 * @brief Provides the definitions and API for BLE encryption modes and key
 * management in Tuya's BLE SDK.
 *
 * This header file defines the encryption modes supported by Tuya's BLE SDK and
 * the structure for managing encryption keys. It includes the enumeration of
 * encryption modes, which range from no encryption to various MD5-based
 * encryption strategies for different BLE operations like pairing, session key
 * exchange, and unbinding commands. Additionally, it declares the structure for
 * holding authentication, user random, and login keys, essential for the
 * encryption and decryption processes in BLE communication.
 *
 * The API and structures defined in this file are crucial for ensuring secure
 * BLE communication between Tuya's IoT devices and the controlling
 * applications, providing a foundation for encrypted data exchange and device
 * authentication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __BLE_ENCRYPTION_H__
#define __BLE_ENCRYPTION_H__

#include "tuya_cloud_types.h"
#include "ble_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ENCRYPTION_MODE_NONE,          // No encryption
    ENCRYPTION_MODE_KEY_11 = 11,   // MD5(authentication key + device UUID + server random number)
    ENCRYPTION_MODE_KEY_12,        // MD5(secret_key_11, srand) result 16 bytes to 32
                                   // characters, take characters 8-24, only for
                                   // pairing request command.
    ENCRYPTION_MODE_KEY_13,        // Reserved
    ENCRYPTION_MODE_KEY_14,        // MD5(16-byte login key + 16-byte secret_key)
                                   // result 16 bytes to 32 characters, take characters
                                   // 8-24
    ENCRYPTION_MODE_SESSION_KEY15, // MD5(16-byte login key, srand) result 16
                                   // bytes to 32 characters, take characters
                                   // 8-24
    ENCRYPTION_MODE_KEY_16,        // MD5(authentication key + md5(device UUID) +
                                   // server random number), used for unbinding command
    ENCRYPTION_MODE_MAX,           // Maximum encryption mode
} ble_key_mode_t;

typedef struct {
    uint8_t *auth_key;
    uint8_t *user_rand;
    uint8_t *login_key;
    uint8_t *sec_key;
    uint8_t *uuid;
    uint8_t *pair_rand;
} ble_crypto_param_t;

/** Clear cached BLE session material between connections. */
void tuya_ble_crypto_reset(void);

uint8_t tuya_ble_encryption(ble_crypto_param_t *p, uint8_t encryption_mode, uint8_t *iv, uint8_t *in_buf,
                            uint32_t in_len, uint32_t *out_len, uint8_t *out_buf);

/**
 * @brief Performs encryption using the Tuya BLE encryption algorithm.
 *
 * This function encrypts the input buffer using the specified encryption mode
 * and parameters.
 *
 * @param p Pointer to the BLE crypto parameters.
 * @param encryption_mode The encryption mode to be used.
 * @param iv Pointer to the initialization vector.
 * @param in_buf Pointer to the input buffer.
 * @param in_len Length of the input buffer.
 * @param out_len Pointer to the variable that will store the length of the
 * output buffer.
 * @param out_buf Pointer to the output buffer.
 *
 * @return Returns 0 on success, or an error code on failure.
 */
uint8_t tuya_ble_decryption(ble_crypto_param_t *p, uint8_t *in_buf, uint32_t in_len, uint32_t *out_len,
                            uint8_t *out_buf);

/**
 * @brief Generates a key for registering with Tuya BLE.
 *
 * This function generates a key for registering with Tuya BLE. The generated
 * key is stored in the output buffer.
 *
 * @param output The buffer to store the generated key.
 * @param auth_key The authentication key.
 * @return true if the key generation is successful, false otherwise.
 */
bool tuya_ble_register_key_generate(uint8_t *output, uint8_t *auth_key);

/**
 * Encrypts the input buffer using the provided key.
 *
 * @param key The encryption key.
 * @param in_buf The input buffer to be encrypted.
 * @param in_len The length of the input buffer.
 * @param out_buf The output buffer to store the encrypted data.
 * @return 0 if the encryption is successful, otherwise an error code.
 */
int tuya_ble_adv_id_encrypt(uint8_t *key, uint8_t *in_buf, uint8_t in_len, uint8_t *out_buf);

/**
 * @brief Encrypts the given input buffer using the provided key.
 *
 * This function encrypts the input buffer using the specified key. The
 * encrypted data is stored in the output buffer.
 *
 * @param key       Pointer to the key used for encryption.
 * @param key_len   Length of the key in bytes.
 * @param in_buf    Pointer to the input buffer to be encrypted.
 * @param in_len    Length of the input buffer in bytes.
 * @param out_buf   Pointer to the output buffer to store the encrypted data.
 *
 * @return          0 if the encryption is successful, otherwise an error code.
 */
int tuya_ble_rsp_id_encrypt(uint8_t *key, uint8_t key_len, uint8_t *in_buf, uint8_t in_len, uint8_t *out_buf);

/**
 * @brief Compresses the given ID using a specific algorithm.
 *
 * This function takes an input ID and compresses it using a specific algorithm.
 * The compressed ID is then stored in the output buffer.
 *
 * @param in Pointer to the input ID buffer.
 * @param out Pointer to the output buffer where the compressed ID will be
 * stored.
 */
void tuya_ble_id_compress(uint8_t *in, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif

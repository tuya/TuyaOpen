/**
 * @file ble_cryption.c
 * @brief This file contains functions related to BLE encryption and decryption
 * processes. It includes key generation, encryption, decryption, and utility
 * functions for handling BLE security operations.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "ble_cryption.h"
#include "tal_api.h"
#include "mix_method.h"
#include "tuya_iot.h"

#define KEY_IN_BUFFER_LEN_MAX 64

static uint8_t service_rand[16] = {0};

static bool ble_key_generate(ble_crypto_param_t *p, uint8_t mode, uint8_t *key_out)
{
    uint16_t len = 0;
    uint8_t uuid_key[16] = {0};
    uint8_t key_out_hex[16] = {0};
    uint8_t key_in_buffer[KEY_IN_BUFFER_LEN_MAX] = {0};
    static uint8_t key_out_key11[16] = {0};

    if (mode >= ENCRYPTION_MODE_MAX) {
        return false;
    }
    if (mode == ENCRYPTION_MODE_NONE) {
        return true;
    }

    memset(key_in_buffer, 0, sizeof(key_in_buffer));

    len = 0;
    memcpy(key_in_buffer + len, p->auth_key, AUTH_KEY_LEN);
    len += AUTH_KEY_LEN;

    if ((16 + PAIR_RANDOM_LEN * 2) > KEY_IN_BUFFER_LEN_MAX) {
        PR_ERR("ble_key_generate err, key length err");
        return false;
    }
    len = 0;

    switch (mode) {
    case ENCRYPTION_MODE_KEY_11:
        memcpy(key_in_buffer + len, p->auth_key, AUTH_KEY_LEN);
        len += AUTH_KEY_LEN;
        memcpy(key_in_buffer + len, p->uuid, 16);
        len += 16;
        memcpy(key_in_buffer + len, service_rand, 16);
        len += 16;
        break;
    case ENCRYPTION_MODE_KEY_12:
        memcpy(key_in_buffer, key_out_key11, 16);
        len += 16;
        memcpy(key_in_buffer + len, p->pair_rand, PAIR_RANDOM_LEN);
        len += PAIR_RANDOM_LEN;
        break;
    case ENCRYPTION_MODE_KEY_14:
        memcpy(key_in_buffer + len, p->login_key, LOGIN_KEY_LEN_16);
        len += LOGIN_KEY_LEN_16;
        memcpy(key_in_buffer + len, p->sec_key, SECRET_KEY_LEN);
        len += SECRET_KEY_LEN;
        break;
    case ENCRYPTION_MODE_SESSION_KEY15:
        memcpy(key_in_buffer + len, p->login_key, LOGIN_KEY_LEN_16);
        len += LOGIN_KEY_LEN_16;
        memcpy(key_in_buffer + len, p->sec_key, SECRET_KEY_LEN);
        len += SECRET_KEY_LEN;
        memcpy(key_in_buffer + len, p->pair_rand, PAIR_RANDOM_LEN);
        len += PAIR_RANDOM_LEN;
        break;
    case ENCRYPTION_MODE_KEY_16:
        // KEY16 = md5(auth key + md5(device uuid) + server rand)
        memcpy(key_in_buffer + len, p->auth_key, AUTH_KEY_LEN);
        len += AUTH_KEY_LEN;
        // md5(uuid)
        tal_md5_ret(p->uuid, 16, uuid_key);
        memcpy(key_in_buffer + len, uuid_key, 16);
        len += 16;
        memcpy(key_in_buffer + len, service_rand, 16);
        len += 16;
        break;
    default:
        break;
    }

    if (len == 0) {
        PR_ERR("ble_key_generat err, len:0.");
        return false;
    }

    memset(key_out_hex, 0, sizeof(key_out_hex));
    tal_md5_ret(key_in_buffer, len, key_out_hex);
    memcpy(key_out, key_out_hex, 16);

    if (ENCRYPTION_MODE_KEY_11 == mode) {
        memcpy(key_out_key11, key_out_hex, 16);
    }

    return true;
}

static uint16_t ble_add_pkcs(uint8_t *p, uint16_t len)
{
    uint8_t pkcs[16];
    uint16_t out_len;

    if (len % 16 == 0) {
        out_len = len;
    } else {
        uint16_t i = 0, cz = 16 - len % 16;
        memset(pkcs, 0, 16);
        for (i = 0; i < cz; i++) {
            pkcs[i] = cz;
        }
        memcpy(p + len, pkcs, cz);
        out_len = len + cz;
    }
    return (out_len);
}

/**
 * @brief Generates a key for registration.
 *
 * This function generates a key for registration using AES-128 ECB encryption.
 *
 * @param output The output buffer to store the generated key.
 * @param auth_key The authentication key used for encryption.
 *
 * @return Returns true if the key generation is successful, false otherwise.
 */
bool tuya_ble_register_key_generate(uint8_t *output, uint8_t *auth_key)
{
    int rt = OPRT_OK;

    rt = tal_aes128_ecb_encode_raw(service_rand, 16, output, auth_key);

    return rt == OPRT_OK ? true : false;
}

/**
 * @brief Encrypts the input buffer using AES-128 ECB mode with a given key.
 *
 * This function takes a key, input buffer, input length, and output buffer as
 * parameters. It performs the following steps:
 * 1. Checks if the input length is within the allowed range.
 * 2. Allocates memory for an intermediate buffer.
 * 3. Calculates the AES key using MD5 hashing algorithm.
 * 4. Converts the input buffer from hexadecimal to string format.
 * 5. Adds PKCS padding to the converted buffer.
 * 6. Performs AES-128 ECB encryption on the padded buffer using the calculated
 * key.
 * 7. Copies the encrypted data to the output buffer.
 * 8. Frees the allocated memory.
 *
 * @param key The encryption key.
 * @param in_buf The input buffer to be encrypted.
 * @param in_len The length of the input buffer.
 * @param out_buf The output buffer to store the encrypted data.
 * @return Returns OPRT_INVALID_PARM if the input length exceeds the buffer
 * size, OPRT_MALLOC_FAILED if memory allocation fails, OPRT_COM_ERROR if the
 * output buffer is too small, or OPRT_OK if the encryption is successful.
 */
int tuya_ble_adv_id_encrypt(uint8_t *key, uint8_t *in_buf, uint8_t in_len, uint8_t *out_buf)
{
    uint16_t pkcslen = 0;
    uint8_t aes_key[16] = {0};

    uint8_t bufsize = 20 * 2 + 16 + 1;
    if (in_len * 2 + 1 > (bufsize - 16)) {
        return OPRT_INVALID_PARM;
    }
    uint8_t *aes_buf = tal_malloc(bufsize * 2);
    if (NULL == aes_buf) {
        return OPRT_MALLOC_FAILED;
    }
    tal_md5_ret(key, MAX_LENGTH_SECKEY, aes_key);
    hex2str(aes_buf, in_buf, in_len);
    pkcslen = ble_add_pkcs(aes_buf, in_len * 2 + 1);
    if (pkcslen > bufsize * 2) {
        tal_free((void *)aes_buf);
        return OPRT_COM_ERROR;
    }
    int rt = tal_aes128_ecb_encode_raw(aes_buf, pkcslen, &aes_buf[bufsize], aes_key);
    if (OPRT_OK == rt) {
        memcpy(out_buf, &aes_buf[bufsize], MAX_LENGTH_SECKEY);
    }
    tal_free((void *)aes_buf);

    return rt;
}

/**
 * @brief Encrypts the input buffer using AES-128 CBC mode with the provided
 * key.
 *
 * This function takes a key, input buffer, and output buffer as parameters. It
 * first calculates the AES key by performing an MD5 hash on the provided key.
 * Then, it initializes the AES IV with the calculated key. Finally, it encrypts
 * the input buffer using AES-128 CBC mode with the key and IV, and stores the
 * result in the output buffer.
 *
 * @param key The key used for encryption.
 * @param key_len The length of the key.
 * @param in_buf The input buffer to be encrypted.
 * @param in_len The length of the input buffer.
 * @param out_buf The output buffer to store the encrypted data.
 * @return The number of bytes written to the output buffer.
 */
int tuya_ble_rsp_id_encrypt(uint8_t *key, uint8_t key_len, uint8_t *in_buf, uint8_t in_len, uint8_t *out_buf)
{
    uint8_t aes_key[16];
    uint8_t aes_iv[16];

    tal_md5_ret(key, key_len, aes_key);
    memcpy(aes_iv, aes_key, 16);

    return tal_aes128_cbc_encode_raw(in_buf, in_len, aes_key, aes_iv, out_buf);
}

/**
 * @brief Performs encryption on the input buffer using the specified encryption
 * mode.
 *
 * @param p                 Pointer to the BLE crypto parameters.
 * @param encryption_mode   The encryption mode to be used.
 * @param iv                Pointer to the initialization vector.
 * @param in_buf            Pointer to the input buffer.
 * @param in_len            Length of the input buffer.
 * @param out_len           Pointer to store the length of the output buffer.
 * @param out_buf           Pointer to the output buffer.
 *
 * @return                  0 if encryption is successful.
 *                          2 if the encryption mode is invalid.
 *                          3 if encryption fails.
 *                          4 if key calculation fails.
 */
uint8_t tuya_ble_encryption(ble_crypto_param_t *p, uint8_t encryption_mode, uint8_t *iv, uint8_t *in_buf,
                            uint32_t in_len, uint32_t *out_len, uint8_t *out_buf)
{
    uint16_t len = 0;
    uint8_t key[16];

    if (encryption_mode >= ENCRYPTION_MODE_MAX) {
        return 2;
    }

    if (encryption_mode == ENCRYPTION_MODE_NONE) {
        memcpy(out_buf, in_buf, in_len);
        *out_len = in_len;
        return 0;
    } else {
        len = ble_add_pkcs(in_buf, in_len);
    }

    memset(key, 0, sizeof(key));
    if (ble_key_generate(p, encryption_mode, key)) {
        *out_len = len;
        int rt = tal_aes128_cbc_encode_raw(in_buf, len, key, iv, out_buf);
        return rt == OPRT_OK ? 0 : 3;
    }

    return 4;
}

/**
 * @brief Decrypts data using the Tuya BLE encryption algorithm.
 *
 * This function decrypts the input data using the Tuya BLE encryption algorithm
 * specified by the given `ble_crypto_param_t` structure. The decrypted data is
 * stored in the `out_buf` buffer.
 *
 * @param p         Pointer to the `ble_crypto_param_t` structure containing the
 *                  encryption parameters.
 * @param in_buf    Pointer to the input data buffer to be decrypted.
 * @param in_len    Length of the input data buffer.
 * @param out_len   Pointer to a variable that will store the length of the
 *                  decrypted data.
 * @param out_buf   Pointer to the output buffer where the decrypted data will
 *                  be stored.
 *
 * @return          Returns `0` on success, or an error code if decryption
 * fails.
 */
uint8_t tuya_ble_decryption(ble_crypto_param_t *p, uint8_t *in_buf, uint32_t in_len, uint32_t *out_len,
                            uint8_t *out_buf)
{
    uint16_t len = 0;
    uint8_t key[16];
    uint8_t IV[16];
    uint8_t mode = 0;

    if (in_len < 17) {
        return 1;
    }

    if (in_buf[0] >= ENCRYPTION_MODE_MAX) {
        return 2;
    }

    if (in_buf[0] == ENCRYPTION_MODE_NONE) {
        len = in_len - 1;
        memcpy(out_buf, in_buf + 1, len);
        *out_len = len;
        return 0;
    }

    len = in_len - 17;

    memset(key, 0, sizeof(key));
    memset(IV, 0, sizeof(IV));

    mode = in_buf[0];
    if (mode == ENCRYPTION_MODE_KEY_11 || mode == ENCRYPTION_MODE_KEY_16) {
        memcpy(service_rand, in_buf + 1, 16); // iv==rand
    }

    if (ble_key_generate(p, mode, key)) {
        memcpy(IV, in_buf + 1, 16);
        *out_len = len;
        int rt = tal_aes128_cbc_decode_raw((uint8_t *)(in_buf + 17), len, key, IV, out_buf);
        return rt == OPRT_OK ? 0 : 3;
    }

    return 4;
}

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
void tuya_ble_id_compress(uint8_t *in, uint8_t *out)
{
    uint8_t i, j;
    uint8_t temp[4];

    for (i = 0; i < 5; i++) {
        for (j = i * 4; j < (i * 4 + 4); j++) {
            if ((in[j] >= 0x30) && (in[j] <= 0x39)) {
                temp[j - i * 4] = in[j] - 0x30;
            } else if ((in[j] >= 0x41) && (in[j] <= 0x5A)) {
                temp[j - i * 4] = in[j] - 0x41 + 36;
            } else if ((in[j] >= 0x61) && (in[j] <= 0x7A)) {
                temp[j - i * 4] = in[j] - 0x61 + 10;
            } else {
            }
        }
        out[i * 3] = temp[0] & 0x3F;
        out[i * 3] <<= 2;
        out[i * 3] |= ((temp[1] >> 4) & 0x03);

        out[i * 3 + 1] = temp[1] & 0x0F;
        out[i * 3 + 1] <<= 4;
        out[i * 3 + 1] |= ((temp[2] >> 2) & 0x0F);

        out[i * 3 + 2] = temp[2] & 0x03;
        out[i * 3 + 2] <<= 6;
        out[i * 3 + 2] |= temp[3] & 0x3F;
    }

    out[15] = 0xFF;
}

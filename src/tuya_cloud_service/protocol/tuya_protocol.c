/**
 * @file tuya_protocol.c
 * @brief Implementation of the Tuya IoT protocol for device communication.
 *
 * This file contains the implementation of the Tuya IoT protocol, which is used
 * for secure communication between Tuya devices and the Tuya cloud platform. It
 * defines the protocol versions, packet structure, and data offsets for
 * different versions of the protocol. The implementation includes support for
 * packet versioning, sequence numbering, command identification, and data
 * encryption/decryption to ensure secure and reliable communication.
 *
 * The protocol handling includes mechanisms for generating random nonces,
 * calculating CRC32 integrity checks, and mixing methods for data encryption.
 * It supports multiple protocol versions to ensure backward compatibility and
 * future-proofing of Tuya IoT devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_protocol.h"
#include "tal_api.h"
#include "crc32i.h"
#include "mix_method.h"
#include "uni_random.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
// PV 2.2 LPV 3.2
#define PV_OFFSET_22_32       0
#define PV_LEN_22_32          3
#define CRC_OFFSET_22_32      (PV_OFFSET_22_32 + PV_LEN_22_32)
#define SEQ_OFFSET_22_32      (CRC_OFFSET_22_32 + sizeof(uint32_t))
#define CMD_FROM_OFFSET_22_32 (SEQ_OFFSET_22_32 + sizeof(uint32_t))
#define DATA_OFFSET_22_32                                                                                              \
    (CMD_FROM_OFFSET_22_32 + sizeof(uint32_t)) // PV/LPV(char[3]) + CRC(uint32_t) + SEQ(uint32_t) +
                                               // CMD_FROM(uint32_t)

// PV 2.3
#define PV23_VERSION_OFFSET  (0)
#define PV23_VERSION_LEN     (3)
#define PV23_SEQ_OFFSET      (PV23_VERSION_LEN)
#define PV23_CMD_FROM_OFFSET (PV23_SEQ_OFFSET + sizeof(uint32_t))
#define PV23_RESERVE_OFFSET  (PV23_CMD_FROM_OFFSET + sizeof(uint32_t))
#define PV23_RESERVE_LEN     (1)
#define PV23_NONCE_OFFSET    (PV23_RESERVE_OFFSET + PV23_RESERVE_LEN)
#define PV23_NONCE_LEN       (12)
#define PV23_DATA_OFFSET     (PV23_NONCE_OFFSET + PV23_NONCE_LEN)
#define PV23_TAG_LEN         (16)
#define PV23_AD_DATA_LEN     (12)
#define PV23_EXCEPT_DATA_LEN (PV23_AD_DATA_LEN + PV23_NONCE_LEN + PV23_TAG_LEN)

/**
 * @brief Generates a serial number for the Tuya protocol packet.
 *
 * This function generates a unique serial number for each Tuya protocol packet.
 *
 * @return The generated serial number.
 */
uint32_t tuya_pack_protocol_serial_no(void)
{
    static uint32_t serial_no = 0;

    if (0 == serial_no) {
        uni_random_bytes((unsigned char *)&serial_no, 4);
    }

    serial_no++;

    return serial_no;
}

static OPERATE_RET __parse_data_with_pv23(const DP_CMD_TYPE_E cmd, const uint8_t *data, const uint32_t len,
                                          const uint8_t *key, char **out_data)
{
    OPERATE_RET op_ret = OPRT_OK;
    if (memcmp(data, TUYA_PV23, PV23_VERSION_LEN) != 0) {
        PR_ERR("verison error, must pv2.3");
        return OPRT_VERSION_FMT_ERR;
    }

    uint32_t seq = 0;
    memcpy(&seq, (data + PV23_SEQ_OFFSET), sizeof(uint32_t));
    seq = UNI_NTOHL(seq);

    uint32_t cmd_from = 0;
    memcpy(&cmd_from, (data + PV23_CMD_FROM_OFFSET), sizeof(uint32_t));
    cmd_from = UNI_NTOHL(cmd_from);

    // reserve_field must clean zore
    uint8_t reserve_field = 0;
    if (memcmp(data + PV23_RESERVE_OFFSET, &reserve_field, PV23_RESERVE_LEN) != 0) {
        PR_ERR("reserve_field must clean zore");
        return OPRT_VERSION_FMT_ERR;
    }

    uint8_t *ad_data = (uint8_t *)(data + 0);
    uint32_t data_len = len - PV23_EXCEPT_DATA_LEN;
    size_t ec_len = 0;
    uint8_t *ec_data = tal_malloc(data_len + 1);
    TUYA_CHECK_NULL_RETURN(ec_data, OPRT_MALLOC_FAILED);

    // decrypt data
    op_ret = mbedtls_cipher_auth_decrypt_wrapper(
        &(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                 .key = (unsigned char *)key,
                                 .key_len = 16,
                                 .nonce = (unsigned char *)(data + PV23_NONCE_OFFSET),
                                 .nonce_len = PV23_NONCE_LEN,
                                 .ad = ad_data,
                                 .ad_len = PV23_AD_DATA_LEN,
                                 .data = (unsigned char *)(data + PV23_DATA_OFFSET),
                                 .data_len = data_len},
        ec_data, &ec_len, (unsigned char *)(data + (len - PV23_TAG_LEN)), PV23_TAG_LEN);
    if (op_ret != OPRT_OK) {
        PR_ERR("mbedtls_cipher_auth_decrypt_wrapper:0x%x", -op_ret);
        *out_data = NULL;
        tal_free(ec_data);
        return op_ret;
    }

    ec_data[ec_len] = 0;

    *out_data = (char *)ec_data;

    return OPRT_OK;
}

static OPERATE_RET __parse_data_with_lpv35(const DP_CMD_TYPE_E cmd, const uint8_t *data, const uint32_t len,
                                           const uint8_t *key, char **out_data)
{
    char pv_buf[4];
    memset(pv_buf, 0, sizeof(pv_buf));
    memcpy(pv_buf, data + PV_OFFSET_22_32, PV_LEN_22_32);

    uint32_t seq = 0;
    memcpy(&seq, (data + SEQ_OFFSET_22_32), sizeof(uint32_t));
    seq = UNI_NTOHL(seq);

    uint32_t cmd_from = 0;
    memcpy(&cmd_from, (data + CMD_FROM_OFFSET_22_32), sizeof(uint32_t));
    cmd_from = UNI_NTOHL(cmd_from);

    uint8_t *ec_data = NULL;
    uint32_t ec_len = len - DATA_OFFSET_22_32;

    ec_data = tal_malloc(ec_len + 1);
    TUYA_CHECK_NULL_RETURN(ec_data, OPRT_MALLOC_FAILED);
    memcpy(ec_data, data + DATA_OFFSET_22_32, ec_len);

    ec_data[ec_len] = 0;

    *out_data = (char *)ec_data;

    return OPRT_OK;
}

/**
 * @brief Parses the protocol data for a given command.
 *
 * This function takes in the command type, data, length, key, and a pointer to
 * store the output data. It parses the protocol data based on the provided
 * command and returns the result in the `out_data` parameter.
 *
 * @param cmd The command type to parse.
 * @param data The input data to be parsed.
 * @param len The length of the input data.
 * @param key The key used for parsing the data.
 * @param out_data A pointer to store the parsed output data.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_INVALID_PARM: Invalid parameter provided.
 *         - OPRT_MALLOC_FAILED: Memory allocation failed.
 *         - OPRT_PARSE_FAILED: Parsing of the protocol data failed.
 */
OPERATE_RET tuya_parse_protocol_data(const DP_CMD_TYPE_E cmd, uint8_t *data, const int len, const char *key,
                                     char **out_data)
{
    if ((NULL == data) || (len < DATA_OFFSET_22_32)) {
        PR_ERR("data is NULL OR Len Invalid %d", len);
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret = OPRT_OK;

    char *pv = NULL;

    if (DP_CMD_LAN == cmd) {
        pv = TUYA_LPV35;
    } else if (DP_CMD_MQ == cmd) {
        pv = TUYA_PV23;
    }

    if (DP_CMD_LAN == cmd) {
        if (0 == strcmp(pv, "3.5")) {
            PR_TRACE("Data From LAN AND V=3.5");
            op_ret = __parse_data_with_lpv35(cmd, data, len, (uint8_t *)key, out_data);
        } else {
            PR_ERR("Data From LAN But No Match Parse %s", pv);
            return OPRT_COM_ERROR;
        }
    } else if (DP_CMD_MQ == cmd) {
        if (0 == strcmp(pv, "2.3")) {
            PR_TRACE("Data From MQTT AND V=2.3");
            op_ret = __parse_data_with_pv23(cmd, data, len, (uint8_t *)key, out_data);
        } else {
            PR_ERR("Data From MQTT But No Match Parse %s", pv);
            return OPRT_COM_ERROR;
        }
    } else {
        PR_ERR("Invlaid Cmd:%d", cmd);
        return OPRT_COM_ERROR;
    }

    return op_ret;
}

static OPERATE_RET __pack_data_with_cmd_pv23(const DP_CMD_TYPE_E cmd, const char *pv, const char *src,
                                             const uint32_t pro, const uint32_t num, const uint8_t *key,
                                             uint8_t **pack_out, uint32_t *out_len)
{
    OPERATE_RET op_ret = OPRT_OK;
    char *out = NULL;
    int offset = 0;

    if (pv == NULL || src == NULL || key == NULL || pack_out == NULL || out_len == NULL) {
        return OPRT_INVALID_PARM;
    }

    PR_TRACE("To:%d src:%s pro:%d num:%d", cmd, src, pro, num);
    // make json data
    int len = strlen(src) + 60;
    out = tal_malloc(len);
    if (NULL == out) {
        PR_ERR("tal_malloc Fails %d", len);
        return OPRT_MALLOC_FAILED;
    }

    int ret = snprintf(out + offset, len - offset, "{\"protocol\":%" PRIu32 ",\"t\":%" PRIu32 ",\"data\":%s", pro,
                       (uint32_t)tal_time_get_posix(), src);
    if (ret < 0 || ret >= len - offset) {
        tal_free(out);
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    offset += ret;

    if (offset > len - 2) {
        tal_free(out);
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    out[offset++] = '}';
    out[offset] = 0;

    PR_TRACE("After Pack:%s offset:%d", out, offset);

    // make pack data
    uint8_t *buf = tal_malloc(offset + PV23_EXCEPT_DATA_LEN);
    if (buf == NULL) {
        tal_free(out);
        return OPRT_MALLOC_FAILED;
    }
    memset(buf, 0, offset + PV23_EXCEPT_DATA_LEN);

    // make head data
    // version
    memcpy(buf + PV23_VERSION_OFFSET, pv, PV_LEN_22_32);

    // seq
    uint32_t tmp = UNI_HTONL(num);
    memcpy(buf + PV23_SEQ_OFFSET, (uint8_t *)(&tmp), sizeof(uint32_t));

    // cmd from
    tmp = UNI_HTONL(0x00000001);
    memcpy(buf + PV23_CMD_FROM_OFFSET, (uint8_t *)(&tmp), sizeof(uint32_t));

    // reserve
    memset(buf + PV23_RESERVE_OFFSET, 0, 1);

    // nonce
    uni_random_string((char *)(buf + PV23_NONCE_OFFSET), PV23_NONCE_LEN);

    // AES GCM encrypt
    size_t encrypt_olen = 0;
    op_ret = mbedtls_cipher_auth_encrypt_wrapper(&(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                                                          .key = (unsigned char *)key,
                                                                          .key_len = 16,
                                                                          .nonce = buf + PV23_NONCE_OFFSET,
                                                                          .nonce_len = PV23_NONCE_LEN,
                                                                          .ad = buf,
                                                                          .ad_len = PV23_AD_DATA_LEN,
                                                                          .data = (unsigned char *)out,
                                                                          .data_len = strlen(out)},
                                                 buf + PV23_DATA_OFFSET, &encrypt_olen, buf + PV23_DATA_OFFSET + offset,
                                                 PV23_TAG_LEN);
    tal_free(out);
    if (op_ret != OPRT_OK) {
        PR_ERR("mbedtls_cipher_auth_encrypt_wrapper:0x%x", -op_ret);
        tal_free(buf);
        return op_ret;
    }

    *pack_out = buf;
    *out_len = PV23_EXCEPT_DATA_LEN + encrypt_olen;

    return OPRT_OK;
}

static OPERATE_RET __pack_data_with_cmd_lpv35(const DP_CMD_TYPE_E cmd, const char *pv, const char *src,
                                              const uint32_t pro, const uint32_t num, const uint8_t *key,
                                              uint8_t **pack_out, uint32_t *out_len)
{
    char *out = NULL;
    int offset = 0;

    if (pv == NULL || src == NULL || key == NULL || pack_out == NULL || out_len == NULL) {
        return OPRT_INVALID_PARM;
    }

    PR_TRACE("To:%d src:%s pro:%d num:%d", cmd, src, pro, num);

    // make json data
    int len = strlen(src) + 60;
    out = tal_malloc(len);
    if (NULL == out) {
        PR_ERR("tal_malloc Fails %d", len);
        return OPRT_MALLOC_FAILED;
    }

    int ret = snprintf(out + offset, len - offset, "{\"protocol\":%" PRIu32 ",\"t\":%" PRIu32 ",\"data\":%s", pro,
                       (uint32_t)tal_time_get_posix(), src);
    if (ret < 0 || ret >= len - offset) {
        tal_free(out);
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    offset += ret;

    if (offset > len - 2) {
        tal_free(out);
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    out[offset++] = '}';
    out[offset] = 0;

    PR_TRACE("After Pack:%s offset:%d", out, offset);

    // make pack data
    uint8_t *buf = tal_malloc(DATA_OFFSET_22_32 + offset + 16);
    if (buf == NULL) {
        tal_free(out);
        return OPRT_MALLOC_FAILED;
    }
    memset(buf, 0, DATA_OFFSET_22_32 + offset + 16);

    // not aes data
    memcpy(buf + DATA_OFFSET_22_32, out, offset);
    tal_free(out);

    *pack_out = buf;
    *out_len = (DATA_OFFSET_22_32 + offset);

    // make head data
    memcpy(buf + PV_OFFSET_22_32, pv, PV_LEN_22_32);

    uint32_t tmp = UNI_HTONL(0x00000000);
    memcpy(buf + CRC_OFFSET_22_32, (uint8_t *)(&tmp), sizeof(uint32_t));

    tmp = UNI_HTONL(num);
    memcpy(buf + SEQ_OFFSET_22_32, (uint8_t *)(&tmp), sizeof(uint32_t));

    tmp = UNI_HTONL(0x00000001);
    memcpy(buf + CMD_FROM_OFFSET_22_32, (uint8_t *)(&tmp), sizeof(uint32_t));

    return OPRT_OK;
}

/**
 * @brief Packs the protocol data for Tuya Cloud service.
 *
 * This function takes the command type, source data, protocol version,
 * encryption key, and outputs the packed protocol data.
 *
 * @param cmd The command type.
 * @param src The source data to be packed.
 * @param pro The protocol version.
 * @param key The encryption key.
 * @param out Pointer to the output packed data.
 * @param out_len Pointer to the length of the output packed data.
 *
 * @return The operation result status.
 *     - OPRT_OK: Operation successful.
 *     - Other error codes: Operation failed.
 */
OPERATE_RET tuya_pack_protocol_data(const DP_CMD_TYPE_E cmd, const char *src, const uint32_t pro, uint8_t *key,
                                    char **out, uint32_t *out_len)
{
    if ((NULL == src) || NULL == out) {
        PR_ERR("Invalid Param");
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret = OPRT_OK;

    char *pv = NULL;

    if (DP_CMD_LAN == cmd) {
        pv = TUYA_LPV35;
    } else if (DP_CMD_MQ == cmd) {
        pv = TUYA_PV23;
    }
    uint32_t num = tuya_pack_protocol_serial_no();

    if (DP_CMD_LAN == cmd) {
        if (0 == strcmp(pv, "3.5")) {
            PR_TRACE("Data To LAN AND V=3.5");
            op_ret = __pack_data_with_cmd_lpv35(cmd, pv, src, pro, num, (uint8_t *)key, (uint8_t **)out, out_len);
        } else {
            PR_ERR("Data To LAN But No Match Parse %s", pv);
            return OPRT_COM_ERROR;
        }
    } else if (DP_CMD_MQ == cmd) {
        if (0 == strcmp(pv, "2.3")) {
            PR_TRACE("Data To MQTT AND V=2.3");
            op_ret = __pack_data_with_cmd_pv23(cmd, pv, src, pro, num, key, (uint8_t **)out, out_len);
        } else {
            PR_ERR("Data To MQTT But No Match Parse %s", pv);
            return OPRT_COM_ERROR;
        }
    } else {
        PR_ERR("Invlaid Cmd:%d", cmd);
        return OPRT_COM_ERROR;
    }

    return op_ret;
}

/**
 * @brief Retrieves the size of the frame buffer for LPV35 frame objects.
 *
 * This function calculates and returns the size of the frame buffer required
 * for the given LPV35 frame object.
 *
 * @param frame_obj Pointer to the LPV35 frame object.
 * @return The size of the frame buffer in bytes.
 */
int lpv35_frame_buffer_size_get(lpv35_frame_object_t *frame_obj)
{
    return (LPV35_FRAME_HEAD_SIZE + sizeof(lpv35_additional_data_t) + LPV35_FRAME_NONCE_SIZE + frame_obj->data_len +
            LPV35_FRAME_TAG_SIZE + LPV35_FRAME_TAIL_SIZE);
}

/**
 * @brief Serializes an LPV35 frame object into a byte array.
 *
 * This function takes a key, key length, input LPV35 frame object, and output
 * byte array as parameters. It serializes the input frame object into the byte
 * array and updates the length of the output array.
 *
 * @param key The key used for serialization.
 * @param key_len The length of the key.
 * @param input The LPV35 frame object to be serialized.
 * @param output The byte array to store the serialized data.
 * @param olen A pointer to the length of the output byte array. This value will
 * be updated with the actual length of the serialized data.
 * @return OPERATE_RET Returns an OPERATE_RET value indicating the success or
 * failure of the serialization process.
 */
OPERATE_RET lpv35_frame_serialize(const uint8_t *key, int key_len, const lpv35_frame_object_t *input, uint8_t *output,
                                  int *olen)
{
    if (key == NULL || key_len == 0 || input == NULL || output == NULL || olen == NULL) {
        PR_ERR("PARAM ERROR");
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET op_ret = OPRT_OK;
    int offset = 0;

    // HEAD
    memcpy(output, LPV35_FRAME_HEAD, LPV35_FRAME_HEAD_SIZE);
    offset += LPV35_FRAME_HEAD_SIZE;

    // AD
    lpv35_additional_data_t ad = {.version = 0,
                                  .sequence = UNI_HTONL(input->sequence),
                                  .type = UNI_HTONL(input->type),
                                  .length = UNI_HTONL(LPV35_FRAME_NONCE_SIZE + input->data_len + LPV35_FRAME_TAG_SIZE)};
    memcpy(output + offset, (uint8_t *)&ad, sizeof(lpv35_additional_data_t));
    offset += sizeof(lpv35_additional_data_t);

    //  tmp nonce
    uint8_t i = 0;
    uint8_t nonce[LPV35_FRAME_NONCE_SIZE];
    for (i = 0; i < LPV35_FRAME_NONCE_SIZE; i++) {
        nonce[i] = uni_random_range(0xFF);
    }
    memcpy(output + offset, &(nonce[0]), LPV35_FRAME_NONCE_SIZE);
    offset += LPV35_FRAME_NONCE_SIZE;

    // TAG buffer
    uint8_t tag[LPV35_FRAME_TAG_SIZE] = {0};

    // AES GCM encrypt
    size_t encrypt_olen = 0;
    op_ret = mbedtls_cipher_auth_encrypt_wrapper(&(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                                                          .key = (unsigned char *)key,
                                                                          .key_len = key_len,
                                                                          .nonce = nonce,
                                                                          .nonce_len = LPV35_FRAME_NONCE_SIZE,
                                                                          .ad = (uint8_t *)(&ad),
                                                                          .ad_len = sizeof(lpv35_additional_data_t),
                                                                          .data = input->data,
                                                                          .data_len = input->data_len},
                                                 output + offset, &encrypt_olen, tag, LPV35_FRAME_TAG_SIZE);
    if (op_ret != OPRT_OK) {
        PR_ERR("mbedtls_cipher_auth_encrypt_wrapper:0x%x", -op_ret);
        return op_ret;
    }
    offset += encrypt_olen;

    // TAG
    memcpy(output + offset, tag, LPV35_FRAME_TAG_SIZE);
    offset += LPV35_FRAME_TAG_SIZE;

    // TAIL
    memcpy(output + offset, LPV35_FRAME_TAIL, LPV35_FRAME_TAIL_SIZE);
    offset += LPV35_FRAME_TAIL_SIZE;
    *olen = offset;

    PR_TRACE("lpv35_frame_buffer_size_get:%d", lpv35_frame_buffer_size_get((lpv35_frame_object_t *)input));
    PR_TRACE("offset:%d", offset);

    return op_ret;
}

/**
 * @brief Parses an LPV35 frame.
 *
 * This function takes the LPV35 frame key, input data, and output object as
 * parameters and parses the LPV35 frame to populate the output object with the
 * parsed data.
 *
 * @param key The LPV35 frame key.
 * @param key_len The length of the LPV35 frame key.
 * @param input The input data containing the LPV35 frame.
 * @param ilen The length of the input data.
 * @param output The output object to store the parsed data.
 *
 * @return The result of the operation. Possible return values are:
 *         - OPRT_OK: The LPV35 frame was successfully parsed.
 *         - OPRT_INVALID_PARM: Invalid parameters were provided.
 *         - OPRT_PARSE_FRAME_ERR: Error occurred while parsing the LPV35 frame.
 */
OPERATE_RET lpv35_frame_parse(const uint8_t *key, int key_len, const uint8_t *input, int ilen,
                              lpv35_frame_object_t *output)
{
    OPERATE_RET op_ret = OPRT_OK;
    int offset = 0;

    if (key == NULL || key_len == 0 || input == NULL || ilen == 0 || output == NULL) {
        PR_ERR("PARAM ERROR");
        return OPRT_INVALID_PARM;
    }

    // head tail verify
    if ((memcmp(input, LPV35_FRAME_HEAD, LPV35_FRAME_HEAD_SIZE) != 0) ||
        (memcmp(input + (ilen - LPV35_FRAME_TAIL_SIZE), LPV35_FRAME_TAIL, LPV35_FRAME_TAIL_SIZE) != 0)) {
        PR_ERR("LPV35 HEAD OR TAIL ERROR");
        return OPRT_VERSION_FMT_ERR;
    }
    offset += LPV35_FRAME_HEAD_SIZE;

    // version
    // uint8_t version = *(input + offset) & 0x0f;
    // PR_DEBUG("version:%d", version);
    offset += LPV35_FRAME_VERSION_SIZE;

    // reserve
    offset += LPV35_FRAME_RESERVE_SIZE;

    // sequence
    memcpy(&output->sequence, input + offset, LPV35_FRAME_SEQUENCE_SIZE);
    output->sequence = UNI_HTONL(output->sequence);
    // PR_DEBUG("sequence:0x%x", output->sequence);
    offset += LPV35_FRAME_SEQUENCE_SIZE;

    // type
    memcpy(&output->type, input + offset, LPV35_FRAME_TYPE_SIZE);
    output->type = UNI_HTONL(output->type);
    // PR_DEBUG("type:0x%x", output->type);
    offset += LPV35_FRAME_TYPE_SIZE;

    // length
    uint32_t length = 0;
    memcpy(&length, input + offset, LPV35_FRAME_DATALEN_SIZE);
    length = UNI_HTONL(length);
    // PR_DEBUG("length:%d", length);
    offset += LPV35_FRAME_DATALEN_SIZE;

    // length verify
    if (length != ilen - offset - LPV35_FRAME_TAIL_SIZE) {
        PR_ERR("length error, length:%d", length);
        return OPRT_COM_ERROR;
    }

    // nonce
    uint8_t nonce[LPV35_FRAME_NONCE_SIZE];
    memcpy(nonce, input + offset, LPV35_FRAME_NONCE_SIZE);
    // tuya_debug_hex_dump("nonce", LPV35_FRAME_NONCE_SIZE, nonce,
    // LPV35_FRAME_NONCE_SIZE);
    offset += LPV35_FRAME_NONCE_SIZE;

    // encryption data
    uint8_t *data = (uint8_t *)(input + offset);
    output->data_len = length - LPV35_FRAME_NONCE_SIZE - LPV35_FRAME_TAG_SIZE;
    offset += output->data_len;

    // tag
    uint8_t tag[LPV35_FRAME_TAG_SIZE];
    memcpy(tag, input + offset, LPV35_FRAME_TAG_SIZE);

    // AD
    lpv35_additional_data_t ad;
    memcpy(&ad, input + LPV35_FRAME_HEAD_SIZE, sizeof(lpv35_additional_data_t));

    // decrypt data
    output->data = tal_malloc(output->data_len + 1);
    TUYA_CHECK_NULL_RETURN(output->data, OPRT_MALLOC_FAILED);
    memset(output->data, 0, output->data_len + 1);
    size_t decrypt_olen = 0;
    op_ret = mbedtls_cipher_auth_decrypt_wrapper(&(const cipher_params_t){.cipher_type = MBEDTLS_CIPHER_AES_128_GCM,
                                                                          .key = (unsigned char *)key,
                                                                          .key_len = key_len,
                                                                          .nonce = nonce,
                                                                          .nonce_len = LPV35_FRAME_NONCE_SIZE,
                                                                          .ad = (uint8_t *)(&ad),
                                                                          .ad_len = sizeof(lpv35_additional_data_t),
                                                                          .data = data,
                                                                          .data_len = output->data_len},
                                                 output->data, &decrypt_olen, tag, LPV35_FRAME_TAG_SIZE);
    if (op_ret != OPRT_OK) {
        PR_ERR("mbedtls_cipher_auth_decrypt_wrapper:0x%x", -op_ret);
        tal_free(output->data);
        output->data = NULL;
        return op_ret;
    }
    output->data_len = (uint32_t)decrypt_olen;
    offset += output->data_len;

    return op_ret;
}

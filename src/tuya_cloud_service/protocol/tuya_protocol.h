/**
 * @file tuya_protocol.h
 * @brief Header file for the Tuya IoT protocol definitions.
 *
 * This header file contains definitions and constants for the Tuya IoT protocol
 * used in device-to-cloud and device-to-device communication. It defines
 * protocol versions, frame types for different operations such as device
 * configuration, time setting, and OTA updates. Additionally, it includes
 * constants for application keys, encryption frame types, and specific protocol
 * commands to manage device functionalities securely and efficiently.
 *
 * The definitions in this file are essential for implementing the communication
 * protocol between Tuya devices and the Tuya cloud platform, ensuring
 * compatibility and supporting advanced features like LAN OTA updates and
 * device schema configuration.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_PROTOCOL__
#define __TUYA_PROTOCOL__
#include "tuya_cloud_types.h"
#include "cipher_wrapper.h"
#include "dp_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUYA_PV23   "2.3"
#define TUYA_LPV35  "3.5"
#define TUYA_BSV    "40.07"
#define APP_KEY_LEN 16

#define FRM_TYPE_AP_ENCRYPTION     (0x11)
#define FRM_TYPE_ENCRYPTION        (0x13)
#define FRM_AP_CFG_WF_V40          (0x14)
#define FRM_AP_CFG_GET_DEV_INFO    (0x16)
#define FRM_AP_CFG_SET_DEV_SCHEMA  (0x17)
#define FRM_AP_CFG_SET_TIME        (0x18)
#define FRM_AP_CFG_SET_ACTIVE_INFO (0x19)
#define FRM_LAN_OTA_START          (0x1A)
#define FRM_LAN_OTA_DATA           (0x1B)
#define FRM_LAN_OTA_FINISH         (0x1C)
#define FRM_LAN_RESET              (0x1D)

#define LPV35_FRAME_HEAD "\x00\x00\x66\x99"
#define LPV35_FRAME_TAIL "\x00\x00\x99\x66"

#define LPV35_FRAME_HEAD_SIZE     4
#define LPV35_FRAME_VERSION_SIZE  1
#define LPV35_FRAME_RESERVE_SIZE  1
#define LPV35_FRAME_SEQUENCE_SIZE 4
#define LPV35_FRAME_TYPE_SIZE     4
#define LPV35_FRAME_DATALEN_SIZE  4
#define LPV35_FRAME_NONCE_SIZE    12
#define LPV35_FRAME_TAG_SIZE      16
#define LPV35_FRAME_TAIL_SIZE     4

#define LPV35_FRAME_MINI_SIZE                                                                                          \
    (LPV35_FRAME_HEAD_SIZE + LPV35_FRAME_VERSION_SIZE + LPV35_FRAME_RESERVE_SIZE + LPV35_FRAME_SEQUENCE_SIZE +         \
     LPV35_FRAME_TYPE_SIZE + LPV35_FRAME_DATALEN_SIZE + LPV35_FRAME_NONCE_SIZE + LPV35_FRAME_TAG_SIZE +                \
     LPV35_FRAME_TAIL_SIZE)

#pragma pack(1)
typedef struct {
    uint8_t version : 4;
    uint8_t reserved : 4;
    uint8_t reserved2;
    uint32_t sequence;
    uint32_t type;
    uint32_t length;
} lpv35_fixed_head_t, tuya_proto_head_t;

typedef lpv35_fixed_head_t lpv35_additional_data_t;

typedef struct {
    uint32_t ret_code;
    uint8_t data[0];
} lpv35_plaintext_data_t;
#pragma pack()

typedef struct {
    uint32_t sequence;
    uint32_t type;
    uint8_t *data;
    uint32_t data_len;
} lpv35_frame_object_t;

typedef dp_cmd_type_t DP_CMD_TYPE_E;
/***********************************************************
 *  Function: parse_data_with_cmd
 *  Input: cmd data len
 *  Output: out_data
 *  Return: OPERATE_RET
 ***********************************************************/

/**
 * @brief parse protocol data
 *
 * @param[in] cmd refer to DP_CMD_TYPE_E
 * @param[in] pv verison
 * @param[in] data origin data
 * @param[in] len data length
 * @param[in] key parse key
 * @param[out] out_data parse out
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_parse_protocol_data(const DP_CMD_TYPE_E cmd, uint8_t *data, const int len, const char *key,
                                     char **out_data);

/**
 * @brief pack protocol data
 *
 * @param[in] cmd refer to DP_CMD_TYPE_E
 * @param[in] pv verison
 * @param[in] src origin data
 * @param[in] pro pro
 * @param[in] num num
 * @param[in] key pack key
 * @param[out] out pack out
 * @param[out] out_len pack out length
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_pack_protocol_data(const DP_CMD_TYPE_E cmd, const char *src, const uint32_t pro, uint8_t *key,
                                    char **out, uint32_t *out_len);
/**
 * @brief add head and tail in lpv35 frame
 *
 * @param[in] key encrypt key
 * @param[in] key_len encrypt key len
 * @param[in] input raw data of lpv35 frame
 * @param[out] output out frame data
 * @param[out] olen out frame data len
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET lpv35_frame_serialize(const uint8_t *key, int key_len, const lpv35_frame_object_t *input, uint8_t *output,
                                  int *olen);

/**
 * @brief lpv35 frame parse
 *
 * @param[in] key decrypt key
 * @param[in] key_len decrypt key len
 * @param[in] input lpv35 frame
 * @param[in] ilen lpv35 frame len
 * @param[out] output decrypt raw lpv35 data
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET lpv35_frame_parse(const uint8_t *key, int key_len, const uint8_t *input, int ilen,
                              lpv35_frame_object_t *output);

/**
 * @brief get lpv35 frame buffer size
 *
 * @param[in] frame_obj lpv35 frame object
 *
 * @return lpv35 frame buffer size
 */
int lpv35_frame_buffer_size_get(lpv35_frame_object_t *frame_obj);

#ifdef __cplusplus
}
#endif
#endif

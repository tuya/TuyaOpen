/**
 * @file tuya_ai_protocol.h
 * @brief This file contains the implementation of Tuya AI protocol processing,
 * including encryption/decryption, message packaging and cloud communication.
 *
 * The Tuya AI protocol module provides core cryptographic functionalities for
 * secure AI service communication, implementing mbedTLS-based encryption (HKDF,
 * ChaCha20) and custom protocol message handling with JSON payload support.
 *
 * Key features include:
 * - Secure communication using mbedTLS cryptographic primitives
 * - Configurable timeout settings (AI_DEFAULT_TIMEOUT_MS)
 * - Cloud service configuration (AI_ATOP_THING_CONFIG_INFO)
 * - Protocol buffer management (AI_ADD_PKT_LEN)
 * - Default business tag handling (AI_DEFAULT_BIZ_TAG)
 * - Socket buffer size configuration (AI_READ_SOCKET_BUF_SIZE)
 * - Integration with Tuya transporter and IoT core services
 * - Cross-platform cipher operations through cipher_wrapper
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_AI_PROTOCOL_H__
#define __TUYA_AI_PROTOCOL_H__

#include <stdint.h>

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_iot_config.h"

#if defined ENABLE_AI_PROTO_DEBUG && (ENABLE_AI_PROTO_DEBUG == 1)
#define AI_PROTO_D(...) PR_DEBUG(__VA_ARGS__)
#else
#define AI_PROTO_D(...) PR_TRACE(__VA_ARGS__)
#endif

/**
 *
 * packet: AI_PACKET_HEAD_T+(iv)+len+payload+sign
 * len:payload+sign
 * payload: AI_PAYLOAD_HEAD_T+(attr_len+AI_ATTRIBUTE_T)+data
 *
 **/

#ifndef AI_MAX_ATTR_NUM
#define AI_MAX_ATTR_NUM 10
#endif

#define AI_KEY_LEN     32
#define AI_RANDOM_LEN  32
#define AI_IV_LEN      16
#define AI_SIGN_LEN    32
#define AI_GCM_TAG_LEN 16
#define AI_UUID_V4_LEN 38

#ifndef AI_MAX_FRAGMENT_LENGTH
#define AI_MAX_FRAGMENT_LENGTH (20 * 1024)
#endif

typedef uint8_t AI_PACKET_SL;
#define AI_PACKET_SL0 0x00 // not encrypted
#define AI_PACKET_SL1 0x01 // not used
#define AI_PACKET_SL2 0x02 // CHACHA20
#define AI_PACKET_SL3 0x03 // CBC
#define AI_PACKET_SL4 0x04 // GCM
#define AI_PACKET_SL5 0x05 // not used

#ifndef AI_PACKET_SECURITY_LEVEL
#define AI_PACKET_SECURITY_LEVEL AI_PACKET_SL4
#endif

typedef uint8_t AI_FRAG_FLAG;
#define AI_PACKET_NO_FRAG    0x00
#define AI_PACKET_FRAG_START 0x01
#define AI_PACKET_FRAG_ING   0x02
#define AI_PACKET_FRAG_END   0x03

typedef uint8_t AI_ATTR_PT;
#define ATTR_PT_U8    0x01
#define ATTR_PT_U16   0x02
#define ATTR_PT_U32   0x03
#define ATTR_PT_U64   0x04
#define ATTR_PT_BYTES 0x05
#define ATTR_PT_STR   0x06

typedef uint8_t AI_PACKET_PT;
#define AI_PT_CLIENT_HELLO      1
#define AI_PT_AUTH_REQ          2
#define AI_PT_AUTH_RESP         3
#define AI_PT_PING              4
#define AI_PT_PONG              5
#define AI_PT_CONN_CLOSE        6
#define AI_PT_SESSION_NEW       7
#define AI_PT_SESSION_CLOSE     8
#define AI_PT_CONN_REFRESH_REQ  9
#define AI_PT_CONN_REFRESH_RESP 10
#define AI_PT_VIDEO             30
#define AI_PT_AUDIO             31
#define AI_PT_IMAGE             32
#define AI_PT_FILE              33
#define AI_PT_TEXT              34
#define AI_PT_EVENT             35

typedef uint16_t AI_ATTR_TYPE;
#define AI_ATTR_CLIENT_TYPE            11
#define AI_ATTR_CLIENT_ID              12
#define AI_ATTR_ENCRYPT_RANDOM         13
#define AI_ATTR_SIGN_RANDOM            14
#define AI_ATTR_MAX_FRAGMENT_LEN       15
#define AI_ATTR_READ_BUFFER_SIZE       16
#define AI_ATTR_WRITE_BUFFER_SIZE      17
#define AI_ATTR_DERIVED_ALGORITHM      18
#define AI_ATTR_DERIVED_IV             19
#define AI_ATTR_USER_NAME              21
#define AI_ATTR_PASSWORD               22
#define AI_ATTR_CONNECTION_ID          23
#define AI_ATTR_CONNECT_STATUS_CODE    24
#define AI_ATTR_LAST_EXPIRE_TS         25
#define AI_ATTR_CONNECT_CLOSE_ERR_CODE 31
#define AI_ATTR_BIZ_CODE               41
#define AI_ATTR_BIZ_TAG                42
#define AI_ATTR_SESSION_ID             43
#define AI_ATTR_SESSION_STATUS_CODE    44
#define AI_ATTR_AGENT_TOKEN            45
#define AI_ATTR_SESSION_CLOSE_ERR_CODE 51
#define AI_ATTR_EVENT_ID               61
#define AI_ATTR_EVENT_TS               62
#define AI_ATTR_STREAM_START_TS        63
#define AI_ATTR_VIDEO_CODEC_TYPE       71
#define AI_ATTR_VIDEO_SAMPLE_RATE      72
#define AI_ATTR_VIDEO_WIDTH            73
#define AI_ATTR_VIDEO_HEIGHT           74
#define AI_ATTR_VIDEO_FPS              75
#define AI_ATTR_AUDIO_CODEC_TYPE       81
#define AI_ATTR_AUDIO_SAMPLE_RATE      82
#define AI_ATTR_AUDIO_CHANNELS         83
#define AI_ATTR_AUDIO_DEPTH            84
#define AI_ATTR_IMAGE_FORMAT           91
#define AI_ATTR_IMAGE_WIDTH            92
#define AI_ATTR_IMAGE_HEIGHT           93
#define AI_ATTR_FILE_FORMAT            101
#define AI_ATTR_FILE_NAME              102
#define AI_ATTR_USER_DATA              111
#define AI_ATTR_SESSION_ID_LIST        112
#define AI_ATTR_CLIENT_TS              113
#define AI_ATTR_SERVER_TS              114

typedef uint8_t ATTR_CLIENT_TYPE;
#define ATTR_CLIENT_TYPE_DEVICE 0x01
#define ATTR_CLIENT_TYPE_APP    0x02

typedef uint8_t AI_ATTR_FLAG;
#define AI_NO_ATTR  0x00
#define AI_HAS_ATTR 0x01

typedef uint16_t AI_STATUS_CODE;
#define AI_CODE_OK                  200
#define AI_CODE_BAD_REQUEST         400
#define AI_CODE_UN_AUTHENTICATED    401
#define AI_CODE_NOT_FOUND           404
#define AI_CODE_REQUEST_TIMEOUT     408
#define AI_CODE_INTERNAL_SERVER_ERR 500
#define AI_CODE_GW_TIMEOUT          504
#define AI_CODE_CLOSE_BY_CLIENT     601
#define AI_CODE_CLOSE_BY_REUSE      602
#define AI_CODE_CLOSE_BY_IO         603
#define AI_CODE_CLOSE_BY_KEEP_ALIVE 604
#define AI_CODE_CLOSE_BY_EXPIRE     605

typedef uint16_t AI_VIDEO_CODEC_TYPE;
#define VIDEO_CODEC_MPEG4  0
#define VIDEO_CODEC_H263   1
#define VIDEO_CODEC_H264   2
#define VIDEO_CODEC_MJPEG  3
#define VIDEO_CODEC_H265   4
#define VIDEO_CODEC_YUV420 5
#define VIDEO_CODEC_YUV422 6
#define VIDEO_CODEC_MAX    99

typedef uint16_t AI_AUDIO_CODEC_TYPE;
#define AUDIO_CODEC_ADPCM   100
#define AUDIO_CODEC_PCM     101
#define AUDIO_CODEC_AACRAW  102
#define AUDIO_CODEC_AACADTS 103
#define AUDIO_CODEC_AACLATM 104
#define AUDIO_CODEC_G711U   105
#define AUDIO_CODEC_G711A   106
#define AUDIO_CODEC_G726    107
#define AUDIO_CODEC_SPEEX   108
#define AUDIO_CODEC_MP3     109
#define AUDIO_CODEC_G722    110
#define AUDIO_CODEC_OPUS    111
#define AUDIO_CODEC_MAX     199
#define AUDIO_CODEC_INVALID 200

typedef uint16_t AI_AUDIO_CHANNELS;
#define AUDIO_CHANNELS_MONO   1
#define AUDIO_CHANNELS_STEREO 2

typedef uint8_t AI_IMAGE_FORMAT;
#define IMAGE_FORMAT_JPEG 1
#define IMAGE_FORMAT_PNG  2

typedef uint8_t AI_FILE_FORMAT;
#define FILE_FORMAT_MP4         1
#define FILE_FORMAT_OGG_OPUS    2
#define FILE_FORMAT_PDF         3
#define FILE_FORMAT_JSON        4
#define FILE_FORMAT_MONITOR_LOG 5
#define FILE_FORMAT_MAP         6

typedef uint16_t AI_EVENT_TYPE;
#define AI_EVENT_START        0x00
#define AI_EVENT_PAYLOADS_END 0x01
#define AI_EVENT_END          0x02
#define AI_EVENT_ONE_SHOT     0x03
#define AI_EVENT_CHAT_BREAK   0x04
#define AI_EVENT_SERVER_VAD   0x05

typedef uint8_t AI_STREAM_TYPE;
#define AI_STREAM_ONE   0x00
#define AI_STREAM_START 0x01
#define AI_STREAM_ING   0x02
#define AI_STREAM_END   0x03

typedef char *AI_SESSION_ID;
typedef char *AI_EVENT_ID;

#pragma pack(1)
typedef struct {
    uint32_t tcp_port;
    uint32_t udp_port;
    uint64_t expire; // unit:s
    uint32_t biz_code;
    char *username;
    char *credential;
    char *client_id;
    char *derived_algorithm;
    char *derived_iv;
    uint32_t host_num;
    char **hosts;
} AI_ATOP_CFG_INFO_T;

typedef union {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    uint8_t *bytes;
    char *str;
} AI_ATTR_VALUE;

typedef struct {
    AI_ATTR_TYPE type;
    AI_ATTR_PT payload_type;
    uint32_t length;
    AI_ATTR_VALUE value;
} AI_ATTRIBUTE_T;

typedef struct {
    AI_ATTR_FLAG attribute_flag : 1;
    AI_PACKET_PT type : 7;
} AI_PAYLOAD_HEAD_T;

typedef struct {
    uint8_t version;
    uint16_t sequence;
    uint8_t iv_flag : 1;
    AI_PACKET_SL security_level : 5;
    AI_FRAG_FLAG frag_flag : 2;
    uint8_t reserve;
} AI_PACKET_HEAD_T;

typedef struct {
    AI_PACKET_PT type;
    uint32_t count;
    AI_ATTRIBUTE_T *attrs[AI_MAX_ATTR_NUM];
	uint32_t total_len;
    uint32_t len;
    char *data;
} AI_SEND_PACKET_T;

typedef struct {
    uint32_t biz_code;
    char *id;
    uint32_t user_len;
    uint8_t *user_data;
} AI_SESSION_NEW_ATTR_T;

typedef struct {
    char *id;
    AI_STATUS_CODE code;
} AI_SESSION_CLOSE_ATTR_T;

typedef struct {
    uint32_t user_len;
    uint8_t *user_data;
    char *session_id_list;
} AI_ATTR_OPTION_T;

typedef struct {
    AI_VIDEO_CODEC_TYPE codec_type;
    uint32_t sample_rate; // unit: Hz
    uint16_t width;
    uint16_t height;
    uint16_t fps;
} AI_VIDEO_ATTR_BASE_T;
typedef struct {
    AI_VIDEO_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;
} AI_VIDEO_ATTR_T;

typedef struct {
    AI_AUDIO_CODEC_TYPE codec_type;
    uint32_t sample_rate; // unit: Hz
    AI_AUDIO_CHANNELS channels;
    uint16_t bit_depth;
} AI_AUDIO_ATTR_BASE_T;
typedef struct {
    AI_AUDIO_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;
} AI_AUDIO_ATTR_T;

typedef struct {
	uint32_t len;
    AI_IMAGE_FORMAT format;
    uint16_t width;
    uint16_t height;
} AI_IMAGE_ATTR_BASE_T;
typedef struct {
    AI_IMAGE_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;
} AI_IMAGE_ATTR_T;

typedef struct {
	uint32_t len;
    AI_FILE_FORMAT format;
    char file_name[128];
} AI_FILE_ATTR_BASE_T;
typedef struct {
    AI_FILE_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;
} AI_FILE_ATTR_T;

typedef struct {
    char *session_id_list;
} AI_TEXT_ATTR_T;

typedef struct {
    char *session_id;
    char *event_id;
    uint64_t end_ts; // unit:ms, used only when event type is AI_EVENT_END
    uint32_t user_len;
    uint8_t *user_data;
} AI_EVENT_ATTR_T;

typedef struct {
    uint16_t id;
    uint8_t reserve : 6;
    AI_STREAM_TYPE stream_flag : 2;
    uint64_t timestamp;
    uint64_t pts;
    uint32_t length;
} AI_VIDEO_HEAD_T, AI_AUDIO_HEAD_T;

typedef struct {
    uint16_t id;
    uint8_t reserve : 6;
    AI_STREAM_TYPE stream_flag : 2;
    uint64_t timestamp;
    uint32_t length;
} AI_IMAGE_HEAD_T;

typedef struct {
    uint16_t id;
    uint8_t reserve : 6;
    AI_STREAM_TYPE stream_flag : 2;
    uint32_t length;
} AI_FILE_HEAD_T, AI_TEXT_HEAD_T;

typedef struct {
    AI_EVENT_TYPE type;
    uint16_t length;
} AI_EVENT_HEAD_T;

typedef struct {
    uint16_t send_ids_length;
    uint16_t *assign_data_ids;
} AI_EVENT_PAYLOADS_END_T;

typedef struct {
    uint8_t *paylaod;
} AI_EVENT_ONE_SHOT_T;
#pragma pack()

/**
 * @brief send ai client hello
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_client_hello(void);

/**
 * @brief send ai auth req
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_auth_req(void);

/**
 * @brief send ai conn close
 *
 * @param[in] code close code
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_conn_close(AI_STATUS_CODE code);

/**
 * @brief read ai packet
 *
 * @param[out] out packet data
 * @param[out] out_len packet data length
 * @param[out] out_frag packet fragment flag
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_pkt_read(char **out, uint32_t *out_len, AI_FRAG_FLAG *out_frag);

/**
 * @brief send ai ping
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_ping(void);

/**
 * @brief send ai packet
 *
 * @param[in] info packet info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_pkt_send(AI_SEND_PACKET_T *info);

/**
 * @brief send ai packet fragment
 *
 * @param[in] info packet info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_pkt_frag_send(AI_SEND_PACKET_T *info);

/**
 * @brief request atop info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_atop_req(void);

/**
 * @brief get atop cfg info
 *
 * @return atop cfg info
 */
AI_ATOP_CFG_INFO_T *tuya_ai_basic_get_atop_cfg(void);

/**
 * @brief ai basic connect
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_connect(void);

/**
 * @brief ai basic disconnect
 *
 */
void tuya_ai_basic_disconnect(void);

/**
 * @brief ai basic refresh req
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_refresh_req(void);

/**
 * @brief ai auth resp
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_auth_resp(void);

/**
 * @brief ai get pkt type
 *
 * @return pkt type
 */
AI_PACKET_PT tuya_ai_basic_get_pkt_type(char *buf);

/**
 * @brief session new
 *
 * @param[in] session session attr
 * @param[in] data session data
 * @param[in] len session data length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_session_new(AI_SESSION_NEW_ATTR_T *session, char *data, uint32_t len);

/**
 * @brief session close
 *
 * @param[in] session_id session id
 * @param[in] code close code
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_session_close(char *session_id, AI_STATUS_CODE code);

/**
 * @brief video packet
 *
 * @param[in] video video attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_video(AI_VIDEO_ATTR_T *video, char *data, uint32_t len);

/**
 * @brief audio packet
 *
 * @param[in] audio audio attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_audio(AI_AUDIO_ATTR_T *audio, char *data, uint32_t len);

/**
 * @brief image packet
 *
 * @param[in] image image attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_image(AI_IMAGE_ATTR_T *image, char *data, uint32_t len);

/**
 * @brief file packet
 *
 * @param[in] file file attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_file(AI_FILE_ATTR_T *file, char *data, uint32_t len);

/**
 * @brief text packet
 *
 * @param[in] text text attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_text(AI_TEXT_ATTR_T *text, char *data, uint32_t len);

/**
 * @brief event packet
 *
 * @param[in] event event attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_event(AI_EVENT_ATTR_T *event, char *data, uint32_t len);

/**
 * @brief get attr value
 *
 * @param[in] de_buf data buffer
 * @param[inout] offset data offset
 * @param[out] attr attribute
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_get_attr_value(char *de_buf, uint32_t *offset, AI_ATTRIBUTE_T *attr);

/**
 * @brief connect refresh resp parse
 *
 * @param[in] de_buf attr buf
 * @param[in] attr_len attr len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_refresh_resp(char *de_buf, uint32_t attr_len);

/**
 * @brief create user attrs
 *
 * @param[in] attr attribute
 * @param[in] attr_num attribute number
 * @param[out] out out data
 * @param[out] out_len out data length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_pack_user_attrs(AI_ATTRIBUTE_T *attr, uint32_t attr_num, uint8_t **out, uint32_t *out_len);

/**
 * @brief parse user attrs
 *
 * @param[in] in in data
 * @param[in] attr_len attr length
 * @param[out] attr_out out attr
 * @param[out] attr_num out attr number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_parse_user_attrs(char *in, uint32_t attr_len, AI_ATTRIBUTE_T **attr_out, uint32_t *attr_num);

/**
 * @brief get uuid v4
 *
 * @param[out] uuid_str uuid string
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_uuid_v4(char *uuid_str);

/**
 * @brief is need attr
 *
 * @param[in] frag_flag fragment flag
 *
 * @return true on need. false on not need
 */
bool tuya_ai_is_need_attr(AI_FRAG_FLAG frag_flag);

/**
 * @brief parse conn close
 *
 * @param[in] de_buf data buffer
 * @param[in] attr_len attribute length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_parse_conn_close(char *de_buf, uint32_t attr_len);

/**
 * @brief parse pong
 *
 * @param[in] data data buffer
 * @param[in] len data length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_pong(char *data, uint32_t len);

/**
 * @brief pkt data free
 *
 * @param[in] data data buffer
 */
void tuya_ai_basic_pkt_free(char *data);

/**
 * @brief set frag flag
 *
 * @param[in] flag fragment flag
 * @note
 * The function should be called after the AI basic protocol is initialized.
 * @return
 */
void tuya_ai_basic_set_frag_flag(bool flag);
#endif
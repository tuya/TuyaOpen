/**
 * @file tuya_ai_protocol.h
 * @author tuya
 * @brief ai protocol
 * @version 0.1
 * @date 2025-03-02
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#ifndef __TUYA_AI_PROTOCOL_H__
#define __TUYA_AI_PROTOCOL_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_types.h"
#include "tuya_iot_config.h"

#ifndef AI_VERSION
#define AI_VERSION 0x01
#endif

#if defined ENABLE_AI_PROTO_DEBUG && (ENABLE_AI_PROTO_DEBUG == 1)
#define AI_PROTO_D(...) PR_DEBUG(__VA_ARGS__)
#else
#define AI_PROTO_D(...) PR_TRACE(__VA_ARGS__)
#endif

#ifndef AI_MAX_ATTR_NUM
#define AI_MAX_ATTR_NUM 10
#endif
#define AI_KEY_LEN 32
#define AI_RANDOM_LEN 32
#define AI_IV_LEN 16
#define AI_SIGN_LEN 32
#define AI_GCM_TAG_LEN 16
#define AI_UUID_V4_LEN 38
#define AI_UUID_SHORT_LEN 24
#define AI_SOLUTION_CODE_LEN 64
#define AI_AGENT_TOKEN_LEN 64
#define AI_TTS_URL_LEN 128

#ifndef AI_MAX_FRAGMENT_LENGTH
#define AI_MAX_FRAGMENT_LENGTH (20*1024)
#endif

typedef uint8_t AI_PACKET_SL;
#define AI_PACKET_SL0 0x00 // not encrypted
#define AI_PACKET_SL1 0x01 // not used
#define AI_PACKET_SL2 0x02 // CHACHA20
#define AI_PACKET_SL3 0x03 // CBC
#define AI_PACKET_SL4 0x04 // GCM
#define AI_PACKET_SL5 0x05 // not used

#define AI_PACKET_RSA 0x06

#ifndef AI_PACKET_SECURITY_LEVEL
#define AI_PACKET_SECURITY_LEVEL AI_PACKET_SL4
#endif

typedef uint8_t AI_FRAG_FLAG;
#define AI_PACKET_NO_FRAG 0x00
#define AI_PACKET_FRAG_START 0x01
#define AI_PACKET_FRAG_ING 0x02
#define AI_PACKET_FRAG_END 0x03

typedef uint8_t AI_ATTR_PT;
#define ATTR_PT_U8   0x01
#define ATTR_PT_U16  0x02
#define ATTR_PT_U32  0x03
#define ATTR_PT_U64  0x04
#define ATTR_PT_BYTES 0x05
#define ATTR_PT_STR   0x06

typedef uint8_t AI_PACKET_PT;
#define AI_PT_CLIENT_HELLO 1
#define AI_PT_AUTH_REQ   2
#define AI_PT_AUTH_RESP  3
#define AI_PT_PING  4
#define AI_PT_PONG  5
#define AI_PT_CONN_CLOSE  6
#define AI_PT_SESSION_NEW  7
#define AI_PT_SESSION_CLOSE  8
#define AI_PT_CONN_REFRESH_REQ  9
#define AI_PT_CONN_REFRESH_RESP  10
#define AI_PT_SESSION_STATE_CHANGE  11
#define AI_PT_DELAY_DISCONNECT  12
#define AI_PT_VIDEO  30
#define AI_PT_AUDIO  31
#define AI_PT_IMAGE  32
#define AI_PT_FILE   33
#define AI_PT_TEXT   34
#define AI_PT_EVENT  35

typedef uint16_t AI_ATTR_TYPE;
#define AI_ATTR_SECURITSUIT_TYPE 10
#define AI_ATTR_CLIENT_TYPE 11
#define AI_ATTR_CLIENT_ID 12
#define AI_ATTR_ENCRYPT_RANDOM 13
#define AI_ATTR_SIGN_RANDOM 14
#define AI_ATTR_MAX_FRAGMENT_LEN 15
#define AI_ATTR_READ_BUFFER_SIZE 16
#define AI_ATTR_WRITE_BUFFER_SIZE 17
#define AI_ATTR_DERIVED_ALGORITHM 18
#define AI_ATTR_DERIVED_IV 19
#define AI_ATTR_PING_INTERVAL 20
#define AI_ATTR_USER_NAME 21
#define AI_ATTR_PASSWORD 22
#define AI_ATTR_CONNECTION_ID 23
#define AI_ATTR_CONNECT_STATUS_CODE 24
#define AI_ATTR_LAST_EXPIRE_TS 25
#define AI_ATTR_CONNECT_CLOSE_ERR_CODE 31
#define AI_ATTR_BIZ_CODE 41
#define AI_ATTR_BIZ_TAG 42
#define AI_ATTR_SESSION_ID 43
#define AI_ATTR_SESSION_STATUS_CODE 44
#define AI_ATTR_AGENT_TOKEN 45
#define AI_ATTR_SESSION_CLOSE_ERR_CODE 51
#define AI_ATTR_SESSION_STATE_CHANGE_CODE 52
#define AI_ATTR_EVENT_ID 61
#define AI_ATTR_EVENT_TS 62
#define AI_ATTR_STREAM_START_TS 63
#define AI_ATTR_DATA_IDS 64
#define AI_ATTR_CMD_DATA 65
#define AI_ATTR_PRIORITY 66
#define AI_ATTR_ASSIGN_DATAS 67
#define AI_ATTR_UNASSIGN_DATAS 68
#define AI_ATTR_VIDEO_PARAMS 70
#define AI_ATTR_VIDEO_CODEC_TYPE 71
#define AI_ATTR_VIDEO_SAMPLE_RATE 72
#define AI_ATTR_VIDEO_WIDTH 73
#define AI_ATTR_VIDEO_HEIGHT 74
#define AI_ATTR_VIDEO_FPS 75
#define AI_ATTR_AUDIO_PARAMS 80
#define AI_ATTR_AUDIO_CODEC_TYPE 81
#define AI_ATTR_AUDIO_SAMPLE_RATE 82
#define AI_ATTR_AUDIO_CHANNELS 83
#define AI_ATTR_AUDIO_DEPTH 84
#define AI_ATTR_AUDIO_FRAME_SIZE 85
#define AI_ATTR_AUDIO_EXTRA_DATA 86
#define AI_ATTR_IMAGE_PARAMS 90
#define AI_ATTR_IMAGE_FORMAT 91
#define AI_ATTR_IMAGE_WIDTH 92
#define AI_ATTR_IMAGE_HEIGHT 93
#define AI_ATTR_FILE_PARAMS 100
#define AI_ATTR_FILE_FORMAT 101
#define AI_ATTR_FILE_NAME 102
#define AI_ATTR_DATA_ID 110
#define AI_ATTR_USER_DATA 111
#define AI_ATTR_SESSION_ID_LIST 112
#define AI_ATTR_CLIENT_TS 113
#define AI_ATTR_SERVER_TS 114
#define AI_ATTR_ASSIGN_CMD_ID 118
#define AI_ATTR_ASSIGN_DATA_IDS 119
#define AI_ATTR_UNASSIGN_DATA_IDS 120

typedef uint8_t ATTR_CLIENT_TYPE;
#define ATTR_CLIENT_TYPE_DEVICE 0x01
#define ATTR_CLIENT_TYPE_APP 0x02

typedef uint8_t AI_ATTR_FLAG;
#define AI_NO_ATTR  0x00
#define AI_HAS_ATTR 0x01

typedef uint16_t AI_STATUS_CODE;
#define AI_CODE_OK 200
#define AI_CODE_BAD_REQUEST 400
#define AI_CODE_UN_AUTHENTICATED 401
#define AI_CODE_NOT_FOUND 404
#define AI_CODE_REQUEST_TIMEOUT 408
#define AI_CODE_INTERNAL_SERVER_ERR 500
#define AI_CODE_GW_TIMEOUT 504
#define AI_CODE_CLOSE_BY_CLIENT 601
#define AI_CODE_CLOSE_BY_REUSE 602
#define AI_CODE_CLOSE_BY_IO 603
#define AI_CODE_CLOSE_BY_KEEP_ALIVE 604
#define AI_CODE_CLOSE_BY_EXPIRE 605

typedef uint16_t AI_STATE_CHANGE_CODE;
#define AI_CODE_AGENT_TOKEN_EXPIRED 1001

typedef uint16_t AI_VIDEO_CODEC_TYPE;
#define VIDEO_CODEC_MPEG4 0
#define VIDEO_CODEC_H263 1
#define VIDEO_CODEC_H264 2
#define VIDEO_CODEC_MJPEG 3
#define VIDEO_CODEC_H265 4
#define VIDEO_CODEC_YUV420 5
#define VIDEO_CODEC_YUV422 6
#define VIDEO_CODEC_MAX 99

typedef uint16_t AI_AUDIO_CODEC_TYPE;
#define AUDIO_CODEC_ADPCM 100
#define AUDIO_CODEC_PCM 101
#define AUDIO_CODEC_AACRAW 102
#define AUDIO_CODEC_AACADTS 103
#define AUDIO_CODEC_AACLATM 104
#define AUDIO_CODEC_G711U 105
#define AUDIO_CODEC_G711A 106
#define AUDIO_CODEC_G726 107
#define AUDIO_CODEC_SPEEX 108
#define AUDIO_CODEC_MP3 109
#define AUDIO_CODEC_G722 110
#define AUDIO_CODEC_OPUS 111
#define AUDIO_CODEC_MAX 199
#define AUDIO_CODEC_INVALID 200

typedef uint16_t AI_AUDIO_CHANNELS;
#define AUDIO_CHANNELS_MONO 1
#define AUDIO_CHANNELS_STEREO 2

typedef uint8_t AI_IMAGE_PAYLOAD_TYPE;
#define IMAGE_PAYLOAD_TYPE_RAW 0
#define IMAGE_PAYLOAD_TYPE_BASE64 1
#define IMAGE_PAYLOAD_TYPE_URL 2

typedef uint8_t AI_IMAGE_FORMAT;
#define IMAGE_FORMAT_JPEG 1
#define IMAGE_FORMAT_PNG 2
#define IMAGE_FORMAT_H264 3         // H264 I frame
#define IMAGE_FORMAT_H265 4         // H265 I frame

typedef uint8_t AI_FILE_PAYLOAD_TYPE;
#define FILE_PAYLOAD_TYPE_RAW 0
#define FILE_PAYLOAD_TYPE_BASE64 1
#define FILE_PAYLOAD_TYPE_URL 2

typedef uint8_t AI_FILE_FORMAT;
#define FILE_FORMAT_MP4 1
#define FILE_FORMAT_OGG_OPUS 2
#define FILE_FORMAT_PDF 3
#define FILE_FORMAT_JSON 4
#define FILE_FORMAT_MONITOR_LOG 5
#define FILE_FORMAT_MAP 6

typedef uint16_t AI_EVENT_TYPE;
#define AI_EVENT_START 0x00
#define AI_EVENT_PAYLOADS_END 0x01
#define AI_EVENT_END 0x02
#define AI_EVENT_ONE_SHOT 0x03
#define AI_EVENT_CHAT_BREAK 0x04
#define AI_EVENT_SERVER_VAD 0x05
#define AI_EVENT_CLEAR_DATA_CACHE 0x06
#define AI_EVENT_MCP_CMD 1000
#define AI_EVENT_SER_TIMEOVER 1001
#define AI_EVENT_UPDATE_CONTEXT 1002
/* device custom event start */
#define AI_EVENT_CHAT_EXIT 0xF000

typedef uint8_t AI_STREAM_TYPE;
#define AI_STREAM_ONE 0x00
#define AI_STREAM_START 0x01
#define AI_STREAM_ING 0x02
#define AI_STREAM_END 0x03

typedef char* AI_SESSION_ID;
typedef char* AI_EVENT_ID;

#pragma pack(1)

typedef struct {
    AI_ATTR_TYPE type;
    AI_ATTR_PT pt;
} AI_ATTR_TYPE_INFO;

typedef struct {
    uint32_t tcp_port;
    uint32_t udp_port;
    uint64_t expire;//unit:s
    char *username;
    char *credential;
    char *client_id;
    char *derived_algorithm;
    char *derived_iv;
    uint32_t host_num;
    char **hosts;
    char *derived_client_id;
    char *rsa_public_key;
} AI_SERVER_CFG_INFO_T;

typedef struct {
    uint32_t code;
    uint32_t send_num;
    char send[AI_BIZ_MAX_NUM];
    uint32_t recv_num;
    char recv[AI_BIZ_MAX_NUM];
} AI_BIZ_CFG_INFO_T;
typedef struct {
    char token[AI_AGENT_TOKEN_LEN];
    char tts_url[AI_TTS_URL_LEN];
    AI_BIZ_CFG_INFO_T biz;
} AI_AGENT_TOKEN_INFO_T;

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
    AI_ATTR_TYPE type;
    uint16_t length;
    AI_ATTR_VALUE value;
} AI_ATTRIBUTE_T_V2;

typedef struct {
    AI_ATTR_FLAG attribute_flag: 1;
    AI_PACKET_PT type: 7;
} AI_PAYLOAD_HEAD_T;

typedef struct {
    uint8_t version;
    uint16_t sequence;
    uint8_t iv_flag: 1;
    AI_PACKET_SL security_level: 5;
    AI_FRAG_FLAG frag_flag: 2;
    uint8_t reserve;
} AI_PACKET_HEAD_T;

typedef struct {
    uint8_t version: 4;
    uint8_t reserve: 2;
    AI_FRAG_FLAG frag_flag: 2;
    uint16_t sequence;
} AI_PACKET_HEAD_T_V2;

typedef enum {
    AI_STAGE_GET_FRAG_OFFSET,
    AI_STAGE_GET_SEQUENCE,
    AI_STAGE_PRE_WRITE,
} AI_STAGE_E;

struct ai_send_packet_t;
struct ai_packet_writer_t;
typedef OPERATE_RET(*AI_PACKET_DATA_UPDATE_CB)(AI_STAGE_E stage, VOID *data, struct ai_send_packet_t *info);
typedef OPERATE_RET(*AI_PACKET_WRITE_CB)(struct ai_packet_writer_t *writer, VOID *buf, uint32_t buf_len);

typedef struct ai_packet_writer_t {
    AI_PACKET_DATA_UPDATE_CB update;    // callback to update internal data
    AI_PACKET_WRITE_CB write;           // callback to write packet data
    VOID *user_data;                    // user data to pass to the writer callback
} AI_PACKET_WRITER_T;

typedef struct ai_send_packet_t {
    AI_PACKET_PT type;
    uint32_t count;
    AI_ATTRIBUTE_T *attrs[AI_MAX_ATTR_NUM];
    uint32_t total_len;
    uint32_t len;
    char *data;
    AI_PACKET_WRITER_T *writer;
} AI_SEND_PACKET_T;

typedef struct {
    uint32_t biz_code;
    uint64_t biz_tag;
    char *token;
    char *id;
    uint32_t user_len;
    uint8_t *user_data;
} AI_SESSION_NEW_ATTR_T;

typedef struct {
    char *id;
    AI_STATUS_CODE code;
} AI_SESSION_CLOSE_ATTR_T;

typedef struct {
    char *id;
    AI_STATE_CHANGE_CODE code;
    uint32_t user_len;
    uint8_t *user_data;
} AI_SESSION_STATE_ATTR_T;

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
    uint16_t frame_size;
} AI_AUDIO_ATTR_BASE_T;
typedef struct {
    AI_AUDIO_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;
} AI_AUDIO_ATTR_T;

typedef struct {
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    AI_IMAGE_PAYLOAD_TYPE type;
#endif
    AI_IMAGE_FORMAT format;
    uint16_t width;
    uint16_t height;
} AI_IMAGE_ATTR_BASE_T;
typedef struct {
    AI_IMAGE_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;
} AI_IMAGE_ATTR_T;

typedef struct {
#if defined(AI_VERSION) && (0x02 == AI_VERSION)
    AI_FILE_PAYLOAD_TYPE type;
#endif
    AI_FILE_FORMAT format;
    char file_name[128];
} AI_FILE_ATTR_BASE_T;
typedef struct {
    AI_FILE_ATTR_BASE_T base;
    AI_ATTR_OPTION_T option;;
} AI_FILE_ATTR_T;

typedef struct {
    char *session_id_list;
} AI_TEXT_ATTR_T;

typedef struct {
    char *session_id;
    char *event_id;
    char *cmd_data;
    char *assign_cmd_id;
    char *assign_datas;
    char *unassign_datas;
    uint64_t end_ts;//unit:ms, used only when event type is AI_EVENT_END
    uint32_t  user_len;
    uint8_t *user_data;
} AI_EVENT_ATTR_T;

typedef struct {
    uint16_t id;
    uint8_t reserve: 6;
    AI_STREAM_TYPE stream_flag: 2;
    uint64_t timestamp;
    uint64_t pts;
    uint32_t length;
} AI_VIDEO_HEAD_T, AI_AUDIO_HEAD_T;

typedef struct {
    uint16_t id;
    AI_STREAM_TYPE stream_flag: 2;
    uint8_t reserve: 4;
    uint64_t timestamp: 42;
} AI_VIDEO_HEAD_T_V2, AI_AUDIO_HEAD_T_V2, AI_IMAGE_HEAD_T_V2;

typedef struct {
    uint16_t id;
    uint8_t reserve: 6;
    AI_STREAM_TYPE stream_flag: 2;
    uint64_t timestamp;
    uint32_t length;
} AI_IMAGE_HEAD_T;

typedef struct {
    uint16_t id;
    uint8_t reserve: 6;
    AI_STREAM_TYPE stream_flag: 2;
    uint32_t length;
} AI_FILE_HEAD_T, AI_TEXT_HEAD_T;

typedef struct {
    uint16_t id;
    uint8_t reserve: 6;
    AI_STREAM_TYPE stream_flag: 2;
} AI_FILE_HEAD_T_V2, AI_TEXT_HEAD_T_V2;

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
 * @param[in] cfg ai server config info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_client_hello(AI_SERVER_CFG_INFO_T *cfg);

/**
 * @brief send ai auth req
 *
 * @param[in] cfg ai server config info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_auth_req(AI_SERVER_CFG_INFO_T *cfg);

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
OPERATE_RET tuya_ai_basic_ping(VOID);

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
 * @brief create user attrs
 *
 * @param[in] in in data
 * @param[in] attr_len attr length
 * @param[out] attr_out out attr
 * @param[out] attr_num out attr number
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
AI_ATTRIBUTE_T* tuya_ai_create_attribute(AI_ATTR_TYPE type, AI_ATTR_PT payload_type, VOID *value, uint32_t len);

/**
 * @brief basic setup
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_setup(VOID);

/**
 * @brief ai basic connect
 *
 * @param[in] cfg ai server config info
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_connect(AI_SERVER_CFG_INFO_T *cfg);

/**
 * @brief ai basic disconnect
 *
 */
VOID tuya_ai_basic_disconnect(VOID);

/**
 * @brief ai basic refresh req
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_refresh_req(VOID);

/**
 * @brief ai auth resp
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_auth_resp(VOID);

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
 * @param[in] len data len
 * @param[in] total_len video total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_video(AI_VIDEO_ATTR_T *video, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer);

/**
 * @brief audio packet
 *
 * @param[in] audio audio attr
 * @param[in] data data
 * @param[in] len data len
 * @param[in] total_len audio total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_audio(AI_AUDIO_ATTR_T *audio, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer);

/**
 * @brief image packet
 *
 * @param[in] image image attr
 * @param[in] data data
 * @param[in] len data len
 * @param[in] total_len image total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_image(AI_IMAGE_ATTR_T *image, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer);

/**
 * @brief file packet
 *
 * @param[in] file file attr
 * @param[in] data data
 * @param[in] len data len
 * @param[in] total_len file total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_file(AI_FILE_ATTR_T *file, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer);

/**
 * @brief text packet
 *
 * @param[in] text text attr
 * @param[in] data data
 * @param[in] len data len
 * @param[in] total_len text total length
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_text(AI_TEXT_ATTR_T *text, char *data, uint32_t len, uint32_t total_len, AI_PACKET_WRITER_T *writer);

/**
 * @brief event packet
 *
 * @param[in] event event attr
 * @param[in] data data
 * @param[in] len len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_event(AI_EVENT_ATTR_T *event, char *data, uint32_t len, AI_PACKET_WRITER_T *writer);

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
 * @param[out] expire expire time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_refresh_resp(char *de_buf, uint32_t attr_len, uint64_t *expire);

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
 * @brief free user attrs
 *
 * @param[in] attr attribute
 */
VOID tuya_free_user_attrs(AI_ATTRIBUTE_T *attr);

/**
 * @brief get uuid v4
 *
 * @param[out] uuid_str uuid string
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_uuid_v4(char *uuid_str);

/**
 * @brief get short uuid
 *
 * @param[out] uuid_str uuid string
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_basic_uuid_short(char *uuid_str);

/**
 * @brief is need attr
 *
 * @param[in] frag_flag fragment flag
 *
 * @return TRUE on need. FALSE on not need
 */
BOOL_T tuya_ai_is_need_attr(AI_FRAG_FLAG frag_flag);

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
VOID tuya_ai_basic_pkt_free(char *data);

/**
 * @brief set frag flag
 *
 * @param[in] flag fragment flag
 * @note
 * The function should be called after the AI basic protocol is initialized.
 * @return
 */
VOID tuya_ai_basic_set_frag(BOOL_T flag);

/**
 * @brief get variable sequence number
 *
 * @param[in] type packet type
 * @param[in] buffer buffer to store the variable integer
 *
 * @return length of the variable integer
 */
uint32_t tuya_ai_basic_get_var_seq(AI_PACKET_PT type, char *buffer);

/**
 * @brief update variable sequence number
 *
 * @param[in] type packet type
 */
VOID tuya_ai_basic_update_var_seq(AI_PACKET_PT type);
#endif
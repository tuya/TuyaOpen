//
//  ====== token format ======
//  {
//      "username" : "12334939:mbzrxpgjys",
//      "password" : "adfsaflsjfldssia",
//      "ttl" : 86400,
//      "uris" : [
//          "turn:1.2.3.4:9991?transport=udp",
//          "turn:1.2.3.4:9992?transport=tcp",
//          "turns:1.2.3.4:443?transport=tcp"
//      ]
// }
#ifndef __TUYA_MEDIA_SERVICE_RTC_H__
#define __TUYA_MEDIA_SERVICE_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define TUYA_P2P_ID_LEN_MAX         80
#define TUYA_P2P_CHANNEL_NUMBER_MAX 320
#define TUYA_P2P_SESSION_NUMBER_MAX 1024
#define TUYA_P2P_VIDEO_BITRATE_MIN  (600)
#define TUYA_P2P_VIDEO_BITRATE_MAX  (4000)

#define TUYA_P2P_ERROR_SUCCESSFUL                         0
#define TUYA_P2P_ERROR_NOT_INITIALIZED                    -1
#define TUYA_P2P_ERROR_ALREADY_INITIALIZED                -2
#define TUYA_P2P_ERROR_TIME_OUT                           -3
#define TUYA_P2P_ERROR_INVALID_ID                         -4
#define TUYA_P2P_ERROR_INVALID_PARAMETER                  -5
#define TUYA_P2P_ERROR_DEVICE_NOT_ONLINE                  -6
#define TUYA_P2P_ERROR_FAIL_TO_RESOLVE_NAME               -7
#define TUYA_P2P_ERROR_INVALID_PREFIX                     -8
#define TUYA_P2P_ERROR_ID_OUT_OF_DATE                     -9
#define TUYA_P2P_ERROR_NO_RELAY_SERVER_AVAILABLE          -10
#define TUYA_P2P_ERROR_INVALID_SESSION_HANDLE             -11
#define TUYA_P2P_ERROR_SESSION_CLOSED_REMOTE              -12
#define TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT             -13
#define TUYA_P2P_ERROR_SESSION_CLOSED_CALLED              -14
#define TUYA_P2P_ERROR_REMOTE_SITE_BUFFER_FULL            -15
#define TUYA_P2P_ERROR_USER_LISTEN_BREAK                  -16
#define TUYA_P2P_ERROR_MAX_SESSION                        -17
#define TUYA_P2P_ERROR_UDP_PORT_BIND_FAILED               -18
#define TUYA_P2P_ERROR_USER_CONNECT_BREAK                 -19
#define TUYA_P2P_ERROR_SESSION_CLOSED_INSUFFICIENT_MEMORY -20
#define TUYA_P2P_ERROR_INVALID_APILICENSE                 -21
#define TUYA_P2P_ERROR_FAIL_TO_CREATE_THREAD              -22
#define TUYA_P2P_ERROR_OUT_OF_SESSION                     -23
#define TUYA_P2P_ERROR_INVALID_PRE_SESSION                -24
#define TUYA_P2P_ERROR_PRE_SESSION_NOT_CONNECTED          -25
#define TUYA_P2P_ERROR_PRE_SESSION_ALREADY_ACTIVE         -26
#define TUYA_P2P_ERROR_PRE_SESSION_NOT_ACTIVE             -27
#define TUYA_P2P_ERROR_PRE_SESSION_SUSPENDED              -28
#define TUYA_P2P_ERROR_OUT_OF_MEMORY                      -29
#define TUYA_P2P_ERROR_HTTP_FAILED                        -30
#define TUYA_P2P_ERROR_PRECONNECT_UNSUPPORTED             -31
#define TUYA_P2P_ERROR_DTLS_HANDSHAKE_FAILED_FINGERPRINT  -32
#define TUYA_P2P_ERROR_GET_TOKEN_TIMEOUT                  -33
#define TUYA_P2P_ERROR_AUTH_FAILED                        -34
#define TUYA_P2P_ERROR_INIT_MBEDTLS_MD_AND_AES_FAILED     -35
#define TUYA_P2P_ERROR_HEARTBEAT_TIMEOUT                  -36
#define TUYA_P2P_ERROR_DTLS_HANDSHAKE_FAILED              -37
#define TUYA_P2P_ERROR_DTLS_HANDSHAKE_TIMEOUT             -38
#define TUYA_P2P_ERROR_REMOTE_NO_RESPONSE                 -39
#define TUYA_P2P_ERROR_PRE_SESSION_RESERVE_TIMEOUT        -40
#define TUYA_P2P_ERROR_RESET                              -41
#define TUYA_P2P_ERROR_UV_TIMER_INIT_FAILED               -42
#define TUYA_P2P_ERROR_SDP_INIT_FAILED                    -43
#define TUYA_P2P_ERROR_SDP_ADD_MEDIA_FAILED               -44
#define TUYA_P2P_ERROR_SDP_ADD_CODEC_FAILED               -45
#define TUYA_P2P_ERROR_CHANNEL_INIT_FAILED                -46
#define TUYA_P2P_ERROR_INVALID_AES_KEY                    -47
#define TUYA_P2P_ERROR_SDP_SET_AES_KEY_FAILED             -48
#define TUYA_P2P_ERROR_INVALID_TOKEN                      -49
#define TUYA_P2P_ERROR_TIME_OUT_NO_ANSWER                 -50
#define TUYA_P2P_ERROR_TIME_OUT_LOCAL_NO_HOST_CAND        -51
#define TUYA_P2P_ERROR_TIME_OUT_LOCAL_NAT                 -52
#define TUYA_P2P_ERROR_TIME_OUT_REMOTE_NAT                -53
#define TUYA_P2P_ERROR_TIME_COWBOY_NO_RESPONSE            -54
#define TUYA_P2P_ERROR_DEVICE_SECRET_MODE                 -102
#define TUYA_P2P_ERROR_DEVICE_CREATE_SEND_THREAD_FAILED   -103
#define TUYA_P2P_ERROR_DEVICE_OUT_OF_SESSION              -104
#define TUYA_P2P_ERROR_DEVICE_AUTH_FAILED                 -105
#define TUYA_P2P_ERROR_DEVICE_SESSION_CLOSED              -106
#define TUYA_P2P_ERROR_DEVICE_CREATE_WEBRTC_THREAD_FAILED -107
#define TUYA_P2P_ERROR_DEVICE_ZOMBI_SESSION               -108
#define TUYA_P2P_ERROR_DEVICE_USER_CLOSE                  -109
#define TUYA_P2P_ERROR_DEVICE_USER_EXIT                   -110
#define TUYA_P2P_ERROR_DEVICE_IN_SECRET_MODE              -111
#define TUYA_P2P_ERROR_DEVICE_CALLING                     -113
#define TUYA_P2P_ERROR_DEVICE_WEBRTC_EXIT                 -300
#define TUYA_P2P_ERROR_REMOTE                             -100

typedef enum tuya_p2p_rtc_security_level {
    TUYA_P2P_SECURITY_LEVEL_0,
    TUYA_P2P_SECURITY_LEVEL_1,
    TUYA_P2P_SECURITY_LEVEL_2,
    TUYA_P2P_SECURITY_LEVEL_3,
    TUYA_P2P_SECURITY_LEVEL_4,
    TUYA_P2P_SECURITY_LEVEL_5,
    /*-----------------------*/
    TUYA_P2P_SECURITY_LEVEL_MAX
} tuya_p2p_rtc_security_level_e;

typedef enum tuya_p2p_rtc_log_level {
    TUYA_P2P_LOG_TRACE,
    TUYA_P2P_LOG_DEBUG,
    TUYA_P2P_LOG_INFO,
    TUYA_P2P_LOG_WARN,
    TUYA_P2P_LOG_ERROR,
    TUYA_P2P_LOG_FATAL
} tuya_p2p_rtc_log_level_e;

typedef enum {
    tuya_p2p_rtc_frame_type_audio,
    tuya_p2p_rtc_frame_type_video_p,
    tuya_p2p_rtc_frame_type_video_i
} tuya_p2p_rtc_frame_type_e;

typedef enum tuya_p2p_rtc_connection_type {
    tuya_p2p_rtc_connection_type_p2p,
    tuya_p2p_rtc_connection_type_webrtc
} tuya_p2p_rtc_connection_type_e;

typedef enum {
    tuya_p2p_rtc_upnp_port_protocol_udp,
    tuya_p2p_rtc_upnp_port_protocol_tcp
} tuya_p2p_rtc_upnp_port_protocol;

#define TUYA_P2P_UPNP_ADDRESS_LENGTH  16 // IP address length
#define TUYA_P2P_UPNP_PROTOCOL_LENGTH 4  // Protocol length tcp or udp

typedef struct _tuya_p2p_rtc_upnp_port_link_t {
    char protocol[TUYA_P2P_UPNP_PROTOCOL_LENGTH];
    int external_port;
    char remount_host[TUYA_P2P_UPNP_ADDRESS_LENGTH];
    int internal_port;
    char internal_client[TUYA_P2P_UPNP_ADDRESS_LENGTH];
    int route_level;
    int index;
    struct _tuya_p2p_rtc_upnp_port_link_t *next;
} tuya_p2p_rtc_upnp_port_link_t;

typedef struct tuya_p2p_rtc_audio_codec {
    char name[64];
    int sample_rate;
    int channel_number;
} tuya_p2p_rtc_audio_codec_t;

typedef struct tuya_p2p_rtc_video_codec {
    char name[64];
    int clock_rate;
} tuya_p2p_rtc_video_codec_t;

typedef struct {
    char *buf;
    uint32_t size;
    uint32_t len;
    uint64_t pts;
    uint64_t timestamp;
    tuya_p2p_rtc_frame_type_e frame_type;
} tuya_p2p_rtc_frame_t;

typedef enum rtc_state {
    RTC_STATE_GET_TOKEN,
    RTC_STATE_P2P_CONNECT,
    RTC_STATE_DTLS_SRTP_KEY_NEGO,
    RTC_STATE_STREAM,
    RTC_STATE_FAILED,
    RTC_STATE_NUMBER,
} rtc_state_e;

typedef enum { RTC_PRE_NOT_ACTIVE = 0, RTC_PRE_ACTIVATING, RTC_PRE_ACTIVE, RTC_PRE_SUSPENDING } rtc_active_state_e;

typedef struct {
    int32_t handle;
    int32_t is_pre;
    rtc_state_e state;
    rtc_active_state_e active_state;
    tuya_p2p_rtc_connection_type_e connection_type;
    tuya_p2p_rtc_audio_codec_t audio_codec;
    tuya_p2p_rtc_video_codec_t video_codec;
    char trace_id[128];
    char session_id[64];
    char sub_dev_id[64];
    int32_t stream_type; // Stream type 0 -- main stream 1 -- sub stream
    int32_t is_replay;
    char start_time[32];
    char end_time[32];
} tuya_p2p_rtc_session_info_t;

// tuya p2p sdk depends on several external services, implemented through callbacks or interfaces:
//  1. Signaling transmission
//      When tuya p2p sdk needs to send signaling, it will call the upper layer implemented tuya_p2p_rtc_signaling_cb_t
//      for transmission
//  2. Signaling reception
//      When the upper layer receives signaling, it is set to tuya p2p sdk through the tuya_p2p_rtc_set_signaling
//      interface
//  3. HTTP service
//      When tuya p2p sdk needs to send logs, it will call the upper layer implemented tuya_p2p_rtc_log_cb_t for
//      transmission

// Signaling callback, used for sending signaling
// remote_id: indicates sending signaling to remote_id
// signaling: starting address of signaling content
// len: length of signaling content
typedef void (*tuya_p2p_rtc_signaling_cb_t)(char *remote_id, char *signaling, uint32_t len);

// Log callback, used for sending logs
// log: starting address of log content
// len: length of log content
typedef void (*tuya_p2p_rtc_log_cb_t)(int level, char *log, uint32_t len);
typedef int (*tuya_p2p_rtc_log_get_level_cb_t)();

// Authentication callback, used to verify if offer is valid
// Use hmac-sha256 to calculate hash of buf content, then calculate base64 encoding of hash result,
// Calculation method: base64(hmac-sha256(key=password, content=buf, length=len))
// Compare the result with md, return 0 if same, return -1 if different
typedef int32_t (*tuya_p2p_rtc_auth_cb_t)(char *buf, uint32_t len, char *md, uint32_t md_len);

// http callback, used for sending http requests
// api: http interface name
// devId: device ID
// content: request content
// content_len: content length
typedef int32_t (*tuya_p2p_rtc_http_cb_t)(char *api, char *devId, char *content, uint32_t content_len);

// crypt callback function
// mode: "aes", "ecb", ...
// crypt: "encrypt", "decrypt"
typedef int (*tuya_p2p_rtc_aes_create_cb_t)(void **handle, char *mode, char *crypt, char *key, int key_bits);
typedef int (*tuya_p2p_rtc_aes_destroy_cb_t)(void *handle);
typedef int (*tuya_p2p_rtc_aes_encrypt_cb_t)(void *handle, int length, char *iv, char *input, char *output);
typedef int (*tuya_p2p_rtc_aes_decrypt_cb_t)(void *handle, int length, char *iv, char *input, char *output);

// upnp callback functions
typedef int (*tuya_p2p_rtc_upnp_alloc_port_cb_t)(tuya_p2p_rtc_upnp_port_protocol protocol, int *local_port,
                                                 char *address, int *port);
typedef int (*tuya_p2p_rtc_upnp_release_port_cb_t)(tuya_p2p_rtc_upnp_port_protocol protocol, int local_port);
typedef int (*tuya_p2p_rtc_upnp_bind_result_cb_t)(tuya_p2p_rtc_upnp_port_protocol protocol, int local_port,
                                                  int error_code);
typedef tuya_p2p_rtc_upnp_port_link_t *(*tuya_p2p_rtc_upnp_request_port_list_cb_t)(
    tuya_p2p_rtc_upnp_port_protocol protocol, unsigned int port);

// session state callback
typedef int (*tuya_p2p_rtc_session_state_cb_t)(char *remote_id, int handle, int is_pre, rtc_state_e state,
                                               rtc_active_state_e active_state, int error);

// localhost callback
// addresses size is 1024
typedef int (*tuya_p2p_rtc_session_get_address_cb_t)(char *address);

typedef struct tuya_p2p_rtc_cb {
    tuya_p2p_rtc_signaling_cb_t on_signaling;      // Signaling callback, application layer sends signaling through mqtt
    tuya_p2p_rtc_signaling_cb_t on_moto_signaling; // Signaling callback, application layer sends signaling through moto
    tuya_p2p_rtc_signaling_cb_t
        on_lan_signaling;         // Signaling callback, application layer sends signaling through LAN signaling channel
    tuya_p2p_rtc_log_cb_t on_log; // Log callback, application layer reports log content to cloud
    tuya_p2p_rtc_log_get_level_cb_t on_log_get_level; // Log callback, application layer reports log content to cloud
    tuya_p2p_rtc_auth_cb_t on_auth;                   // Authentication callback, used by device side
    tuya_p2p_rtc_http_cb_t on_http; // http callback, used by app side, called when tuya p2p sdk needs http interface
    struct {
        // External encryption/decryption interface, set to NULL if not needed
        tuya_p2p_rtc_aes_create_cb_t on_create;
        tuya_p2p_rtc_aes_destroy_cb_t on_destroy;
        tuya_p2p_rtc_aes_encrypt_cb_t on_encrypt;
        tuya_p2p_rtc_aes_decrypt_cb_t on_decrypt;
    } aes;
    struct {
        tuya_p2p_rtc_upnp_alloc_port_cb_t on_alloc;     // Get UPN mapped port information
        tuya_p2p_rtc_upnp_release_port_cb_t on_release; // Release UPNP mapped port
        tuya_p2p_rtc_upnp_bind_result_cb_t on_bind;     // Feedback binding mapped port result to UPNP module
        tuya_p2p_rtc_upnp_request_port_list_cb_t
            on_request_port_list; // Get multi-level router mapped address and port information
    } upnp;

    tuya_p2p_rtc_session_state_cb_t on_session_state;
    tuya_p2p_rtc_session_get_address_cb_t on_get_address;
} tuya_p2p_rtc_cb_t;

// p2p sdk initialization parameters
typedef struct tuya_p2p_rtc_options {
    char local_id[TUYA_P2P_ID_LEN_MAX]; // Local id, device side fills device id, client side fills uid
    tuya_p2p_rtc_cb_t cb;               // Callback interface
    uint32_t max_channel_number;        // Maximum number of channels per connection
    uint32_t max_session_number;        // Allow simultaneous initiation or reception of several connections
    uint32_t max_pre_session_number;    // Allow simultaneous initiation or reception of several pre-connections
    uint32_t send_buf_size[TUYA_P2P_CHANNEL_NUMBER_MAX]; // Send buffer size for each channel, in bytes
    uint32_t recv_buf_size[TUYA_P2P_CHANNEL_NUMBER_MAX]; // Receive buffer size for each channel, in bytes
    uint32_t video_bitrate_kbps; // Device side fills video bitrate, client side does not need to set
    uint32_t preconnect_enable;  // Whether to enable pre-connection, 1: enable, 0: disable
    uint32_t fragement_len;      // Data sending interface fragmentation length
} tuya_p2p_rtc_options_t;

// Get p2p sdk version number
uint32_t tuya_p2p_rtc_get_version();
// Get p2p sdk capability set
uint32_t tuya_p2p_rtc_get_skill();
// p2p sdk initialization
int32_t tuya_p2p_rtc_init(tuya_p2p_rtc_options_t *opt);
int32_t tuya_p2p_rtc_close(int32_t handle, int32_t reason);
// p2p sdk deinitialization
int32_t tuya_p2p_rtc_deinit();
int32_t tuya_p2p_rtc_reset(tuya_p2p_rtc_options_t *opt);
// Send signaling to p2p sdk, called when application layer receives p2p signaling
// remote_id: not used yet
// msg: signaling content, json format string
// msglen: signaling length
// return value: 0
int32_t tuya_p2p_rtc_set_signaling(char *remote_id, char *msg, uint32_t msglen);
// Initiate pre-connection to the other party
// remote_id: id receiving mqtt signaling
// dev_id: device id
// return value: 0
int32_t tuya_p2p_rtc_pre_connect(char *remote_id, char *dev_id);
// Close pre-connection
// remote_id: id receiving mqtt signaling
// return value: 0
int32_t tuya_p2p_rtc_pre_connect_close(char *remote_id, int32_t reason);
// Initiate connection to the other party, atop interface is triggered by p2p library, called after connect
// remote_id: id receiving mqtt signaling
// dev_id: device id
// skill: device capability set, skill field obtained from cloud, json format string
// skill_len: skill length
// token: p2pConfig field obtained from cloud, json format string
// token_len: token length
// trace_id: full link log id, can be empty
// lan_mode: LAN priority mode
// timeout_ms: connection timeout, usually 15s
// return value: < 0 indicates failure, >=0 indicates successful connection, similar to socket fd
int32_t tuya_p2p_rtc_connect_v2(char *remote_id, char *dev_id, char *skill, uint32_t skill_len, char *token,
                                uint32_t token_len, char *trace_id, int lan_mode, int timeout_ms);
// Initiate connection to the other party
// remote_id: id of the other party, usually device id
// token: p2pConfig field obtained from cloud, json format string
// token_len: token length
// trace_id: full link log id, can be empty
// lan_mode: LAN priority mode
// timeout_ms: connection timeout, usually 15s
// return value: < 0 indicates failure, >=0 indicates successful connection, similar to socket fd
int32_t tuya_p2p_rtc_connect(char *remote_id, char *token, uint32_t token_len, char *trace_id, int lan_mode,
                             int timeout_ms);
// Cancel the connection being attempted, after calling this interface, tuya_p2p_rtc_connect interface returns failure
// immediately; Already established connections are not affected
int32_t tuya_p2p_rtc_connect_break();
// Cancel the specified connection being attempted, after calling this interface, tuya_p2p_rtc_connect interface returns
// failure immediately; Already established connections are not affected trace_id: full link log id, specify the
// connection to cancel
int32_t tuya_p2p_rtc_connect_break_one(char *trace_id);
// http response
// api: http interface name
// code: http response status code
// result: http response content
// result_len: result length
int32_t tuya_p2p_rtc_set_http_result(char *api, uint32_t code, char *result, uint32_t result_len);
// http response
// api: http interface name
// code: http response status code
// content: on_http request content
// content_len: content length
// result: http response content
// result_len: result length
int32_t tuya_p2p_rtc_set_http_result_v2(char *api, uint32_t code, char *dev_id, char *content, uint32_t content_len,
                                        char *result, uint32_t result_len);
// Accept a connection, similar to socket's accept interface
// return value: < 0 indicates failure, >= 0 indicates success
int32_t tuya_p2p_rtc_listen();
// Interrupt listen, after calling this interface, tuya_p2p_rtc_listen returns failure immediately
int32_t tuya_p2p_rtc_listen_break();
// Get connection related information, used by device side, to determine if the corresponding connection is p2p
// connection or webrtc connection
int32_t tuya_p2p_rtc_get_session_info(int32_t handle, tuya_p2p_rtc_session_info_t *info);
// Get session list
// Returns json format string, need to call tuya_p2p_rtc_free_session_list to release
char *tuya_p2p_rtc_get_session_list();
// Release session list
// session_list: return value of tuya_p2p_rtc_get_session_list
void tuya_p2p_rtc_free_session_list(char *session_list);
// Check if connection is still alive
// handle: session handle
// return value: < 0 indicates session abnormal, 0 indicates session normal
int32_t tuya_p2p_rtc_check(int32_t handle);
// Close connection or suspend pre-connection
int32_t tuya_p2p_rtc_close(int32_t handle, int32_t reason);
// Send audio/video frame (webrtc specific)
int32_t tuya_p2p_rtc_send_frame(int32_t handle, tuya_p2p_rtc_frame_t *frame);
// Send audio frame (webrtc specific)
int32_t tuya_p2p_rtc_recv_frame(int32_t handle, tuya_p2p_rtc_frame_t *frame);
// Send data to the other party
// handle: connection handle
// channel_id: channel number
// buf: pointer to content to be sent
// len: length of data to be sent
// timeout_ms: interface blocking time, in milliseconds
// return value
// >=0: number of bytes sent successfully
// TUYA_P2P_ERROR_TIME_OUT: send timeout
// others: send failed, and connection has been disconnected
int32_t tuya_p2p_rtc_send_data(int32_t handle, uint32_t channel_id, char *buf, int32_t len, int32_t timeout_ms);
// Receive data
// handle: connection handle
// channel_id: channel number
// buf: receive buffer
// len: length of receive buffer
// timeout_ms: interface blocking time, in milliseconds
// return value
// 0: receive successful, *len indicates number of bytes received
// TUYA_P2P_ERROR_TIME_OUT: receive timeout
// others: receive failed, and connection has been disconnected
int32_t tuya_p2p_rtc_recv_data(int32_t handle, uint32_t channel_id, char *buf, int32_t *len, int32_t timeout_ms);
void tuya_p2p_rtc_notify_exit();
// Check the current send/receive buffer status of a connection:
// handle: connection handle
// channel_id: channel number
// write_size: after function returns, updated to current bytes written to send buffer but not sent successfully
// read_size: after function returns, updated to current bytes received successfully but not read by application layer
// send_free_size: after function returns, updated to remaining space in send buffer
// return value: undefined
int32_t tuya_p2p_rtc_check_buffer(int32_t handle, uint32_t channel_id, uint32_t *write_size, uint32_t *read_size,
                                  uint32_t *send_free_size);
// Notify p2p sdk that a device just came online
// Mainly used for low-power devices
int32_t tuya_p2p_rtc_set_remote_online(char *remote_id);
int32_t tuya_p2p_getwaitsnd(int32_t handle, uint32_t channel_id);
int32_t tuya_p2p_log_set_level(tuya_p2p_rtc_log_level_e level);

#ifdef __cplusplus
}
#endif

#endif

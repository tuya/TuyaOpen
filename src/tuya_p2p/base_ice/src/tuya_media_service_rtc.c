#include "tuya_media_service_rtc.h"
#include "tal_mutex.h"
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
// #include <sys/eventfd.h>
#ifndef __APPLE__
#include <sys/prctl.h>
#endif
#include "ikcp.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include "tuya_log.h"
#include "tuya_misc.h"
#include "cJSON.h"
#if (MBEDTLS_VERSION_NUMBER < 0x03000000)
#include "mbedtls/certs.h"
#endif
#include "mbedtls/ssl.h"
#include "mbedtls/timing.h"
#include "tuya_sdp.h"
#include "pj_ice.h"
#include "pj_sync_condition.h"
#include <pjmedia/sdp.h>
#include "tal_log.h"

#define IKCP_PACKET_HEADER_SIZE       24
#define TUYA_P2P_SEND_BUFFER_SIZE_MAX (800 * 1024)
#define TUYA_P2P_SEND_BUFFER_SIZE_MIN (50 * 1024)
#define TUYA_P2P_RECV_BUFFER_SIZE_MAX (800 * 1024)
#define TUYA_P2P_RECV_BUFFER_SIZE_MIN (50 * 1024)
#define RTC_SESSION_RUN_INTERVAL_MS   5
#define SRTP_MASTER_KEY_LENGTH        16
#define SRTP_MASTER_SALT_LENGTH       14
#define SRTP_MASTER_LENGTH            (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)
#define ENCRYPT_MD5_LEN               16

// CMD is transmitted using kcp's channel number field, and kcp uses little endian
#define RTC_CHANNEL_CMD   (0x010000F3)
#define RTC_CMD_SIGNALING (0x0001)

#define P2P_UPLOAD_LOG_MASK_OPEN      0x01
#define P2P_UPLOAD_LOG_MASK_HANDSHAKE 0x02
#define P2P_UPLOAD_LOG_MASK_CLOSE     0x04
#define P2P_UPLOAD_LOG_MASK_ACTIVATE  0x08

#define RTC_TOKEN_REFRESH_INTERVAL_SECONDS 600

#define P2P_DEFAULT_FRAGEMENT_LEN 1300

typedef enum rtc_session_close_reason {
    RTC_SESSION_CLOSE_REASON_OK = 0,
    RTC_SESSION_CLOSE_REASON_ICE_FAILED = 1,
    RTC_SESSION_CLOSE_REASON_DTLS_HANDSHAKE_FAILED = 2,
    RTC_SESSION_CLOSE_REASON_LOCAL_CANCEL = 3,
    RTC_SESSION_CLOSE_REASON_LOCAL_CLOSE = 4,
    RTC_SESSION_CLOSE_REASON_REMOTE_CLOSE = 5,
    RTC_SESSION_CLOSE_REASON_KEEPALIVE_TIMEOUT = 6,
    RTC_SESSION_CLOSE_REASON_AUTH_FAILED = 7,
    RTC_SESSION_CLOSE_REASON_MEMORY_ALLOC = 8,
    RTC_SESSION_CLOSE_REASON_DTLS_HANDSHAKE_FAILED_FINGERPRINT = 9,
    RTC_SESSION_CLOSE_REASON_ICE_UDP_TCP_ALL_FAILED = 10,
    RTC_SESSION_CLOSE_REASON_RESET = 11,
    RTC_SESSION_CLOSE_REASON_REFUSED = 12,
    RTC_SESSION_CLOSE_REASON_PRE_CMD_TIMEOUT = 13,
    RTC_SESSION_CLOSE_REASON_GET_TOKEN_TIMEOUT = 14,
    RTC_SESSION_CLOSE_REASON_RESERVE_TIMEOUT = 15,
    RTC_SESSION_CLOSE_REASON_PRECONNECT_UNSUPPORTED = 16,
    RTC_SESSION_CLOSE_REASON_HTTP_FAILED = 17,
    RTC_SESSION_CLOSE_REASON_PRE_MESS = 18,
    RTC_SESSION_CLOSE_REASON_SECURITY_NEGOTIATE_FAIL = 19,
    RTC_SESSION_CLOSE_REASON_INIT_MBEDTLS_MD_AND_AES = 20,
    RTC_SESSION_CLOSE_REASON_DTLS_HANDSHAKE_TIMEOUT = 21,
    RTC_SESSION_CLOSE_REASON_UNDEFINED = 99
} rtc_session_close_reason_e;

typedef struct tagTuyaBuf {
    char *base;
    size_t len;
} tuya_uv_buf_t;

typedef struct tuya_p2p_rtc_dtls_cert {
    unsigned char cert[8 * 1024];
    unsigned char pkey[8 * 1024];
    char fingerprint[1024];
    int cert_len;
    int pkey_len;
} tuya_p2p_rtc_dtls_cert_t;

typedef struct rtc_channel {
    struct tuya_p2p_rtc_session *rtc;
    // tuya_mbuf_queue_t *send_queue;
    // tuya_mbuf_queue_t *recv_queue;
    int has_receiver;
    ikcpcb *kcp;
    int channel_id;
    uint32_t has_sent_to_tcp;
    uint32_t highest_seq_tcp_has_sent;
    int64_t write_bytes;
    int64_t read_bytes;
    int64_t send_bytes;
    int64_t recv_bytes;
    int64_t socket_send_bytes;
    int64_t socket_recv_bytes;
    int64_t first_send_time_ms;
    int64_t first_write_time_ms;
    int64_t first_read_time_ms;
    int64_t first_read_try_time_ms;
    int64_t first_data_time_ms;

    void *aes_ctx_enc;
    void *aes_ctx_dec;
} rtc_channel_t;

#if (MBEDTLS_VERSION_NUMBER > 0x03000000)
#define MBEDTLS_TLS_SRTP_MAX_KEY_MATERIAL_LENGTH 60
typedef struct dtls_srtp_keys {
    unsigned char master_secret[48];
    unsigned char randbytes[64];
    mbedtls_tls_prf_types tls_prf_type;
} dtls_srtp_keys;
#endif
typedef struct rtc_dtls {
    int inited;
    int remote_cert_verified;
    tuya_p2p_rtc_dtls_cert_t cert;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt x509_crt;
    mbedtls_pk_context pkey;
    mbedtls_timing_delay_context timer;
#if (MBEDTLS_VERSION_NUMBER > 0x03000000)
    dtls_srtp_keys dtls_srtp_keying;
#endif
} rtc_dtls_t;

// typedef struct rtc_srtp {
//     int inited;
//     int srtp_profile;
//     unsigned char remote_policy_key[SRTP_MASTER_LENGTH];
//     unsigned char local_policy_key[SRTP_MASTER_LENGTH];
//     srtp_policy_t remote_policy;
//     srtp_policy_t local_policy_audio;
//     srtp_policy_t local_policy_video;
//     srtp_policy_t local_policy_video_rtx;
//     srtp_ctx_t *srtp_sess_in;
//     srtp_ctx_t *srtp_sess_out;
// } rtc_srtp_t;

typedef struct rtc_transport {
    struct {
        uint32_t ice;
        uint32_t udp;
        uint32_t tcp;
    } water_level;
} rtc_transport_t;

typedef enum {
    MSG_TYPE_SIGNALING,
    MSG_TYPE_CONTROL,
    MSG_TYPE_REPORT,
    MSG_TYPE_CERT,
    MSG_TYPE_HTTP,
    MSG_TYPE_STATE,
} msg_type_e;

typedef enum { PJ_ROLE_CALLER, PJ_ROLE_CALLEE } pj_role_e;

typedef struct tuya_p2p_rtc_session_cfg {
    // tuya_uv_loop_t *loop;
    uint32_t offline_timeout_seconds;
    uint32_t connect_limit_time_ms;
    uint32_t lan_mode;
    int p2p_skill;
    int preconnect_enable;
    int is_pre;
    int is_webrtc;
    pj_role_e role;
    char local_id[64]; // Added by Langdon
    char remote_id[64];
    char session_id[64];
    char connect_session[64];
    char connect_api[64];
    char dev_id[64];
    char node_id[64];
    char moto_id[64];
    char trace_id[256];
    char auth[128];
    char ice_ufrag[32];
    char ice_password[32];
    char aes_key[64];
    uint32_t channel_number;
    int32_t stream_type;
    int32_t is_replay;
    char start_time[32];
    char end_time[32];
    char ice_server_tokens[2048];
    char udp_server_tokens[2048];
    char tcp_server_tokens[2048];
    // rtc_token_t *rtc_token;
    // rtc_token_type_e token_type;
    // tuya_p2p_rtc_security_level_e security_level;
    int security_level;
} rtc_session_cfg_t;

typedef struct tuya_p2p_rtc_session {
    tuya_p2p_rtc_cb_t cb; // Callback interface

    int ref_cnt;
    pthread_mutex_t ref_lock;

    sync_cond_t syncCondExit;

    rtc_sdp_t local_sdp;
    rtc_sdp_t remote_sdp;
    pjmedia_sdp_session *pLocalSdp;
    pjmedia_sdp_session *pRemoteSdp;
    pthread_mutex_t channel_lock;
    rtc_channel_t *channels;
    // tuya_uv_timer_t *te;
    // rtc_state_entry_t state;
    rtc_session_cfg_t cfg;
    void *queue[2];

    int active_handle;
    int local_cmd_seq;

    // kcp channel
    unsigned char aes_key[16];
    unsigned char iv[16];
    mbedtls_md_info_t *md_info;
    mbedtls_md_context_t md_ctx;

    struct {
        char recv_buf[4096];
        uint32_t recv_already;
    } cmd_channel;

    pj_ice_session_t *pIce;
    pthread_t tid;
    bool bQuitKCPThread;
} tuya_p2p_rtc_session_t;

tuya_p2p_rtc_options_t g_options;
static uint32_t g_uP2PSkill = TUYA_P2P_SDK_SKILL_BASIC /*TUYA_P2P_SDK_SKILL_NUMBER*/;
tuya_p2p_rtc_session_t *g_pRtcSession = NULL;
MUTEX_HANDLE            g_p2p_session_mutex = NULL;
rtc_session_cfg_t cfg;
pj_ice_session_cfg_t iceSessionCfg;

static const unsigned char KCP_CMD_PUSH = 81; // cmd: push data
static const unsigned char KCP_CMD_ACK = 82;  // cmd: ack
static const unsigned char KCP_CMD_WASK = 83; // cmd: window probe (ask)
static const unsigned char KCP_CMD_WINS = 84; // cmd: window size (tell)

sync_cond_t g_syncCond;

#define KA_INTERVAL 300
#define THIS_FILE   "tuya_media_service_rtc2.c"

void ice_on_ice_complete(pj_ice_strans *ice_st, pj_ice_strans_op op, pj_status_t status);
void ice_on_new_candidate(pj_ice_strans *ice_st, const pj_ice_sess_cand *cand, pj_bool_t last);
void ice_on_rx_data(pj_ice_strans *ice_st, unsigned comp_id, void *buffer, pj_size_t size,
                    const pj_sockaddr_t *src_addr, unsigned src_addr_len);

tuya_p2p_rtc_session_t *ctx_session_create(rtc_session_cfg_t *cfg, rtc_state_e state, int32_t *err_code);
void ctx_session_destroy(tuya_p2p_rtc_session_t *rtc);
void ctx_session_channel_set_send_time(struct rtc_channel *chan);
int ctx_session_channel_process_data(struct rtc_channel *chan, char *data, int len);
int ctx_session_channel_process_pkt(void *user, int length, const char *input, char *output);
int ctx_session_send_sdp(tuya_p2p_rtc_session_t *rtc, rtc_session_cfg_t *cfg); // For example, send Answer SDP
int ctx_session_send_candidate(tuya_p2p_rtc_session_t *rtc, rtc_session_cfg_t *cfg, char *cand_str);
int ctx_session_add_remote_candidate(tuya_p2p_rtc_session_t *rtc, rtc_sdp_t *remote_sdp, char *candidate);
int ctx_session_send_suspend_resp(tuya_p2p_rtc_session_t *rtc, int error);
int ctx_session_send_disconnect(tuya_p2p_rtc_session_t *rtc, int32_t close_reason_local, rtc_session_close_reason_e close_reason);
int ctx_session_send_signaling(tuya_p2p_rtc_session_t *rtc, char *signaling);
char *ctx_signaling_add_path(char *signaling, char *path);
int tuya_p2p_rtc_channels_init(tuya_p2p_rtc_session_t *rtc);
void tuya_p2p_rtc_channels_destroy(tuya_p2p_rtc_session_t *rtc);

void rtc_process_kcp_data(tuya_p2p_rtc_session_t *rtc, const tuya_uv_buf_t *pkt);

int rtc_init_mbedtls_md_and_aes(tuya_p2p_rtc_session_t *rtc);
int rtc_channel_aes_init(rtc_channel_t *chan);
int rtc_crypt_encrypt_aes_128_cbc(struct tuya_p2p_rtc_session *rtc, void *ctx, size_t length, unsigned char *iv,
                                  const unsigned char *input, unsigned char *output);
int rtc_crypt_decrypt_aes_128_cbc(struct tuya_p2p_rtc_session *rtc, void *ctx, size_t length, unsigned char *iv,
                                  const unsigned char *input, unsigned char *output);
int rtc_channel_aes_uninit(struct rtc_channel *chan);

void *rtc_worker_thread(void *arg);

void rtc_ref_cnt_add(tuya_p2p_rtc_session_t *rtc);
void rtc_ref_cnt_del(tuya_p2p_rtc_session_t *rtc);
int rtc_ref_cnt_get(tuya_p2p_rtc_session_t *rtc);

int32_t tuya_p2p_rtc_init(tuya_p2p_rtc_options_t *opt)
{
    sync_cond_init(&g_syncCond);
    memcpy(&g_options, opt, sizeof(tuya_p2p_rtc_options_t));
    g_options.preconnect_enable = false; // Disable the use of pre-connection
    // g_pRtcSession->cb = opt->cb;

    // int i;
    // for (i = 0; i < TUYA_P2P_CHANNEL_NUMBER_MAX; i++) {
    //     if (i < g_options.max_channel_number) {
    //         int send_buffer_size_min = TUYA_P2P_SEND_BUFFER_SIZE_MIN;
    //         int recv_buffer_size_min = TUYA_P2P_RECV_BUFFER_SIZE_MIN;
    //         switch (i) {
    //         case 1:
    //         case 3:
    //             send_buffer_size_min = 500 * 1024;
    //             recv_buffer_size_min = 500 * 1024;
    //             break;
    //         default:
    //             break;
    //         }
    //         ctx->opt.send_buf_size[i] = ctx->opt.send_buf_size[i] < send_buffer_size_min
    //                                         ? send_buffer_size_min
    //                                         : ctx->opt.send_buf_size[i];
    //         ctx->opt.recv_buf_size[i] = ctx->opt.recv_buf_size[i] < recv_buffer_size_min
    //                                         ? recv_buffer_size_min
    //                                         : ctx->opt.recv_buf_size[i];
    //         ctx->opt.send_buf_size[i] = ctx->opt.send_buf_size[i] > TUYA_P2P_SEND_BUFFER_SIZE_MAX
    //                                         ? TUYA_P2P_SEND_BUFFER_SIZE_MAX
    //                                         : ctx->opt.send_buf_size[i];
    //         ctx->opt.recv_buf_size[i] = ctx->opt.recv_buf_size[i] > TUYA_P2P_RECV_BUFFER_SIZE_MAX
    //                                         ? TUYA_P2P_RECV_BUFFER_SIZE_MAX
    //                                         : ctx->opt.recv_buf_size[i];
    //     } else {
    //         ctx->opt.send_buf_size[i] = 0;
    //         ctx->opt.recv_buf_size[i] = 0;
    //     }
    // }

    // if (ctx->opt.video_bitrate_kbps < TUYA_P2P_VIDEO_BITRATE_MIN) {
    //     ctx->opt.video_bitrate_kbps = TUYA_P2P_VIDEO_BITRATE_MIN;
    // }
    // if (ctx->opt.video_bitrate_kbps > TUYA_P2P_VIDEO_BITRATE_MAX) {
    //     ctx->opt.video_bitrate_kbps = TUYA_P2P_VIDEO_BITRATE_MAX;
    // }

    return 0;
}

int32_t tuya_p2p_rtc_close(int32_t handle, int32_t reason)
{
    if (g_pRtcSession == NULL) {
        return TUYA_P2P_ERROR_NOT_INITIALIZED;
    }
    tuya_p2p_log_info("rtc session %08x close\n", handle);
    ctx_session_send_disconnect(g_pRtcSession, reason, RTC_SESSION_CLOSE_REASON_LOCAL_CLOSE);
    tuya_p2p_log_info("rtc session %08x close over\n", handle);
    return 0;
}

static int tuya_p2p_process_signal_msg(char *msg, int msglen)
{
    (void)msglen;
    cJSON *root = cJSON_Parse(msg);
    if (root == NULL) {
        printf("invalid webrtc signaling: not a json\n=========\n%s\n==========\n", msg);
        return -1;
    }

    // parse header
    cJSON *el_header = cJSON_GetObjectItemCaseSensitive(root, "header");
    if (!cJSON_IsObject(el_header)) {
        printf("invalid signaling: invalid json, no header field\n");
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return -1;
    }
    cJSON *el_from = cJSON_GetObjectItemCaseSensitive(el_header, "from");
    cJSON *el_to = cJSON_GetObjectItemCaseSensitive(el_header, "to");
    cJSON *el_node_id = cJSON_GetObjectItemCaseSensitive(el_header, "sub_dev_id");
    cJSON *el_sessionid = cJSON_GetObjectItemCaseSensitive(el_header, "sessionid");
    cJSON *el_trace_id = cJSON_GetObjectItemCaseSensitive(el_header, "trace_id");
    cJSON *el_moto_id = cJSON_GetObjectItemCaseSensitive(el_header, "moto_id");
    cJSON *el_path = cJSON_GetObjectItemCaseSensitive(el_header, "path");
    cJSON *el_type = cJSON_GetObjectItemCaseSensitive(el_header, "type");
    cJSON *el_is_pre = cJSON_GetObjectItemCaseSensitive(el_header, "is_pre");
    cJSON *el_p2p_skill = cJSON_GetObjectItemCaseSensitive(el_header, "p2p_skill");
    cJSON *el_security_level = cJSON_GetObjectItemCaseSensitive(el_header, "security_level");
    if ((!cJSON_IsString(el_from)) || (!cJSON_IsString(el_to)) || (!cJSON_IsString(el_sessionid)) ||
        (!(cJSON_IsString(el_type)))) {
        printf("invalid signaling: invalid header\n");
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return -1;
    }
    char *remote_id = el_from->valuestring;
    char *local_id = el_to->valuestring;
    char *session_id = el_sessionid->valuestring;
    char *trace_id = cJSON_IsString(el_trace_id) ? el_trace_id->valuestring : "";
    char *moto_id = cJSON_IsString(el_moto_id) ? el_moto_id->valuestring : "";
    char *str_path = cJSON_IsString(el_path) ? el_path->valuestring : "";
    char *node_id = cJSON_IsString(el_node_id) ? el_node_id->valuestring : "";
    char *type = el_type->valuestring;
    int is_pre = cJSON_IsNumber(el_is_pre) ? el_is_pre->valueint : 0;
    int p2p_skill = cJSON_IsNumber(el_p2p_skill) ? el_p2p_skill->valueint : 0;

    // parse msg
    cJSON *el_msg = cJSON_GetObjectItemCaseSensitive(root, "msg");
    if (!cJSON_IsObject(el_msg)) {
        printf("invalid signaling: invalid json, no msg field\n");
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return -1;
    }
    cJSON *el_replay = cJSON_GetObjectItemCaseSensitive(el_msg, "replay");
    cJSON *el_sdp = cJSON_GetObjectItemCaseSensitive(el_msg, "sdp");
    cJSON *el_token = cJSON_GetObjectItemCaseSensitive(el_msg, "token");
    cJSON *el_udp_token = cJSON_GetObjectItemCaseSensitive(el_msg, "udp_token");
    cJSON *el_tcp_token = cJSON_GetObjectItemCaseSensitive(el_msg, "tcp_token");
    cJSON *el_candidate = cJSON_GetObjectItemCaseSensitive(el_msg, "candidate");
    cJSON *el_mode = cJSON_GetObjectItemCaseSensitive(el_msg, "mode");
    cJSON *el_auth = cJSON_GetObjectItemCaseSensitive(el_msg, "auth");
    if (el_auth == NULL) {
        el_auth = cJSON_GetObjectItemCaseSensitive(el_msg, "Auth");
    }
    cJSON *el_stream_type = cJSON_GetObjectItemCaseSensitive(el_msg, "stream_type");
    char *auth = cJSON_IsString(el_auth) ? el_auth->valuestring : "";
    int32_t stream_type = cJSON_IsNumber(el_stream_type) ? el_stream_type->valueint : -1;
    cJSON *el_preconnect = cJSON_GetObjectItemCaseSensitive(el_msg, "preconnect");

    printf("process signaling %s\n", type);
    // create new session if necessary
    // static pj_session_cfg_t cfg;
    // tuya_p2p_rtc_session_t *rtc = ctx_session_get(ctx, remote_id, session_id);
    if (g_pRtcSession == NULL && strcmp(type, "offer") == 0) {
        memset(&cfg, 0, sizeof(cfg));

        if (cJSON_IsString(el_mode) && strcmp(el_mode->valuestring, "webrtc") == 0) {
            cfg.is_webrtc = 1;
        } else {
            // cfg.channel_number = ctx->opt.max_channel_number; // todo: set channel number
        }

        if (!cJSON_IsArray(el_token) && !cJSON_IsObject(el_udp_token) && !cJSON_IsObject(el_tcp_token)) {
            printf("invalid signaling: invalid json, no token field\n");
            return -1;
        } else {
            if (cJSON_IsArray(el_token)) {
                char *token_str = cJSON_PrintUnformatted(el_token);
                if (token_str != NULL) {
                    snprintf(cfg.ice_server_tokens, sizeof(cfg.ice_server_tokens), "%s",
                             token_str); // Parse ICE server information from cloud
                    cJSON_free(token_str);
                }
            }
            if (cJSON_IsObject(el_udp_token)) {
                char *token_str = cJSON_PrintUnformatted(el_udp_token);
                if (token_str != NULL) {
                    snprintf(cfg.udp_server_tokens, sizeof(cfg.udp_server_tokens), "%s", token_str);
                    cJSON_free(token_str);
                }
            }
            if (cJSON_IsObject(el_tcp_token)) {
                char *token_str = cJSON_PrintUnformatted(el_tcp_token);
                if (token_str != NULL) {
                    snprintf(cfg.tcp_server_tokens, sizeof(cfg.tcp_server_tokens), "%s", token_str);
                    cJSON_free(token_str);
                }
            }
        }

        if (cJSON_IsNumber(el_security_level)) {
            printf("security_level in offer: %d\n", el_security_level->valueint);
        } else {
            printf("no security_level in offer, use default L3\n");
        }
        int peer_security_level =
            cJSON_IsNumber(el_security_level) ? el_security_level->valueint : 3 /*TUYA_P2P_SECURITY_LEVEL_3*/;
        // if (__check_security_level(peer_security_level) < 0) {
        // ctx_session_send_disconnect_v2(ctx,
        //                                cfg.moto_id,
        //                                ctx->opt.local_id,
        //                                remote_id,
        //                                node_id,
        //                                session_id,
        //                                trace_id,
        //                                0,
        //                                RTC_SESSION_CLOSE_REASON_SECURITY_NEGOTIATE_FAIL,
        //                                path);
        // return -1;
        // }

        // create rtc session
        // cfg.loop = &ctx->loop;
        cfg.offline_timeout_seconds = 30;
        cfg.role = PJ_ROLE_CALLEE;
        // cfg.channel_number = ctx->opt.max_channel_number;
        cfg.connect_limit_time_ms = 15000;
        cfg.stream_type = stream_type >= 0 ? stream_type : 0;
        cfg.is_pre = is_pre;
        cfg.p2p_skill = 0 /*p2p_skill*/;
        cfg.preconnect_enable = 0;
        // if (cJSON_IsBool(el_preconnect)) {
        //     cfg.preconnect_enable = el_preconnect->valueint;
        // }
        cfg.security_level = peer_security_level;
        snprintf(cfg.local_id, sizeof(cfg.local_id), "%s", local_id);
        snprintf(cfg.remote_id, sizeof(cfg.remote_id), "%s", remote_id);
        snprintf(cfg.session_id, sizeof(cfg.session_id), "%s", session_id);
        snprintf(cfg.node_id, sizeof(cfg.node_id), "%s", node_id);
        snprintf(cfg.trace_id, sizeof(cfg.trace_id), "%s", trace_id);
        snprintf(cfg.moto_id, sizeof(cfg.moto_id), "%s", moto_id);
        snprintf(cfg.auth, sizeof(cfg.auth), "%s", auth);
        tuya_p2p_misc_rand_string(cfg.ice_ufrag, 5);
        tuya_p2p_misc_rand_string(cfg.ice_password, 25);

        if (cJSON_IsObject(el_replay)) {
            cJSON *el_is_replay = cJSON_GetObjectItemCaseSensitive(el_replay, "is_replay");
            cJSON *el_start_time = cJSON_GetObjectItemCaseSensitive(el_replay, "start_time");
            cJSON *el_end_time = cJSON_GetObjectItemCaseSensitive(el_replay, "end_time");
            if (cJSON_IsNumber(el_is_replay)) {
                cfg.is_replay = el_is_replay->valueint;
            }
            if (cJSON_IsString(el_start_time)) {
                snprintf(cfg.start_time, sizeof(cfg.start_time), "%s", el_start_time->valuestring);
            }
            if (cJSON_IsString(el_end_time)) {
                snprintf(cfg.end_time, sizeof(cfg.end_time), "%s", el_end_time->valuestring);
            }
        }

        int32_t err_code = 0;
        tal_mutex_create_init(&g_p2p_session_mutex);
        g_pRtcSession = ctx_session_create(&cfg, RTC_STATE_P2P_CONNECT, &err_code);
        memcpy(&g_pRtcSession->cb, &g_options.cb, sizeof(g_options.cb));

        iceSessionCfg.cb.ice_on_rx_data = ice_on_rx_data;
        iceSessionCfg.cb.ice_on_ice_complete = ice_on_ice_complete;
        iceSessionCfg.cb.ice_on_new_candidate = ice_on_new_candidate;
        iceSessionCfg.rolechar = 'o';
        iceSessionCfg.local_ufrag = cfg.ice_ufrag;
        iceSessionCfg.local_passwd = cfg.ice_password;
        iceSessionCfg.user_data = g_pRtcSession;
        memcpy(iceSessionCfg.server_tokens, cfg.ice_server_tokens, sizeof(iceSessionCfg.server_tokens));
        pj_ice_session_create(&iceSessionCfg, &g_pRtcSession->pIce);
        pj_ice_session_init(g_pRtcSession->pIce, &iceSessionCfg);

        pthread_create(&g_pRtcSession->tid, NULL, rtc_worker_thread, g_pRtcSession);
        tuya_p2p_log_info("ctx_session_create\n");
    }

    if (g_pRtcSession == NULL) {
        tuya_p2p_log_info("can not find rtc session\n");
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return -1;
    }

    if (strcmp(type, "candidate") == 0) {
        if (!cJSON_IsString(el_candidate)) {
            printf("invalid signaling: type: candidate\n");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        tal_mutex_lock(g_p2p_session_mutex);
        ctx_session_add_remote_candidate(g_pRtcSession, &g_pRtcSession->remote_sdp, el_candidate->valuestring);
        tal_mutex_unlock(g_p2p_session_mutex);
    } else if (strcmp(type, "offer") == 0) {
        if (!cJSON_IsString(el_sdp)) {
            printf("invalid signaling: type: sdp\n");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        char *buf = el_sdp->valuestring;
        tal_mutex_lock(g_p2p_session_mutex);
        tuya_p2p_rtc_sdp_decode(&g_pRtcSession->remote_sdp, buf);
        tuya_p2p_rtc_sdp_negotiate(&g_pRtcSession->local_sdp, &g_pRtcSession->remote_sdp, type);
        ctx_session_send_sdp(g_pRtcSession, &cfg); // Send Answer_SDP to peer
        tal_mutex_unlock(g_p2p_session_mutex);
    } else if ((strcmp(type, "answer") == 0)) {
        if (!cJSON_IsString(el_sdp)) {
            tuya_p2p_log_debug("invalid signaling: type: sdp\n");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        char *buf = el_sdp->valuestring;
        tal_mutex_lock(g_p2p_session_mutex);
        tuya_p2p_rtc_sdp_decode(&g_pRtcSession->remote_sdp, buf);
        tuya_p2p_rtc_sdp_negotiate(&g_pRtcSession->local_sdp, &g_pRtcSession->remote_sdp, type);
        tal_mutex_unlock(g_p2p_session_mutex);
    } else if (strcmp(type, "disconnect") == 0) {
        cJSON *jclose_reason_local = cJSON_GetObjectItemCaseSensitive(el_msg, "close_reason_local");
        cJSON *jclose_reason = cJSON_GetObjectItemCaseSensitive(el_msg, "close_reason");
        int close_reason =
            cJSON_IsNumber(jclose_reason) ? jclose_reason->valueint : 99 /*RTC_SESSION_CLOSE_REASON_UNDEFINED*/;
        int close_reason_local = cJSON_IsNumber(jclose_reason_local) ? jclose_reason_local->valueint : 0;
        //tal_mutex_lock(g_p2p_session_mutex);
        ctx_session_destroy(g_pRtcSession);
        g_pRtcSession = NULL;
        //tal_mutex_unlock(g_p2p_session_mutex);
        tal_mutex_release(g_p2p_session_mutex);
        g_p2p_session_mutex = NULL;
    } else if (strcmp(type, "activate") == 0) {
        cJSON *el_handle = cJSON_GetObjectItemCaseSensitive(el_msg, "handle");
        cJSON *el_seq = cJSON_GetObjectItemCaseSensitive(el_msg, "seq");
        if (!cJSON_IsNumber(el_handle) || !cJSON_IsNumber(el_seq)) {
            tuya_p2p_log_debug("invalid signaling: type: handle or seq\n");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        //     int active_handle = el_handle->valueint;
        //     int seq = el_seq->valueint;
        //     if (rtc->remote_cmd_seq >= seq) {
        //         tuya_p2p_log_warn(
        //             "rtc session %08x got old %s: %d >= %d\n", rtc->handle, type, rtc->remote_cmd_seq, seq);
        //         return -1;
        //     }
        //     rtc->remote_cmd_seq = seq;
        //     if (rtc->active_state == RTC_PRE_ACTIVE) {
        //         if (rtc->active_handle == active_handle) {
        //             tuya_p2p_log_warn(
        //                 "rtc session %08x got repeated activation:%d\n", rtc->handle, active_handle);
        //             ctx_session_send_activate_resp(ctx, rtc, TUYA_P2P_ERROR_SUCCESSFUL);
        //         } else {
        //             ctx_session_send_activate_resp(ctx, rtc, TUYA_P2P_ERROR_PRE_SESSION_ALREADY_ACTIVE);
        //         }
        //         return -1;
        //     }
        //     if (rtc->active_state != RTC_PRE_NOT_ACTIVE) {
        //         tuya_p2p_log_warn("rtc session %08x state %d\n", rtc->handle, rtc->active_state);
        //         ctx_session_send_activate_resp(ctx, rtc, TUYA_P2P_ERROR_PRE_SESSION_ALREADY_ACTIVE);
        //         return -1;
        //     }
        //     int error = TUYA_P2P_ERROR_SUCCESSFUL;
        //     if (rtc->cfg.is_pre == 0) {
        //         error = TUYA_P2P_ERROR_INVALID_PRE_SESSION;
        //     } else if (rtc->state.state != RTC_STATE_STREAM) {
        //         error = TUYA_P2P_ERROR_PRE_SESSION_NOT_CONNECTED;
        //     } else if (ctx_get_current_session_number(ctx) >= ctx->opt.max_session_number) {
        //         error = TUYA_P2P_ERROR_OUT_OF_SESSION;
        //     }
        //     if (error == TUYA_P2P_ERROR_SUCCESSFUL) {
        //         rtc->log.activate_time = tuya_p2p_misc_get_timestamp_ms();
        //         rtc->log.activate_resp_time = 0;
        //         rtc->log.suspend_time = 0;
        //         rtc->log.suspend_resp_time = 0;
        //         rtc->connect_break_flag = 0;
        //         rtc->log.close_reason_local = 0;
        //         rtc->log.close_reason_remote = 0;
        //         rtc->active_state = RTC_PRE_ACTIVE;
        //         rtc->active_handle = active_handle;
        //         rtc->handle |= rtc->active_handle << 16;
        //         rtc->has_notified = 0;
        //         rtc->cfg.is_pre = 0;
        //         rtc->cfg.role = RTC_ROLE_CALLEE;
        //         tuya_p2p_rtc_channels_reset(rtc);
        //         ctx_session_on_state_change(rtc);
        //         ctx_new_session_activate(ctx, rtc, TUYA_P2P_ERROR_SUCCESSFUL);
        //     }
        //     ctx_session_send_activate_resp(ctx, rtc, error);
    } else if (strcmp(type, "suspend") == 0) {
        cJSON *el_handle = cJSON_GetObjectItemCaseSensitive(el_msg, "handle");
        cJSON *el_reason = cJSON_GetObjectItemCaseSensitive(el_msg, "reason");
        cJSON *el_seq = cJSON_GetObjectItemCaseSensitive(el_msg, "seq");
        if (!cJSON_IsNumber(el_handle) || !cJSON_IsNumber(el_reason) || !cJSON_IsNumber(el_seq)) {
            tuya_p2p_log_debug("invalid signaling: type: handle or seq\n");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        int active_handle = el_handle->valueint;
        int reason = el_reason->valueint;
        int seq = el_seq->valueint;
        // if (rtc->remote_cmd_seq >= seq) {
        //     tuya_p2p_log_warn("rtc session %08x got old %s: %d >= %d\n", rtc->handle, type, rtc->remote_cmd_seq,
        //     seq); return -1;
        // }
        // rtc->remote_cmd_seq = seq;
        // uint32_t pre_session_number = ctx_get_pre_session_number(ctx);
        // uint32_t pre_session_number_remote = ctx_get_pre_session_number_by_remote(ctx, rtc->cfg.remote_id);
        // tuya_p2p_log_info("remote %s pre session number %d\n", rtc->cfg.remote_id, pre_session_number_remote);
        tal_mutex_lock(g_p2p_session_mutex);
        tuya_p2p_rtc_session_t *rtc = g_pRtcSession;
        rtc->active_handle = active_handle;
        ctx_session_send_suspend_resp(rtc, TUYA_P2P_ERROR_SUCCESSFUL);
        tal_mutex_unlock(g_p2p_session_mutex);
    }
    if (root != NULL) {
        cJSON_Delete(root);
    }
    return 0;
}

int32_t tuya_p2p_rtc_set_signaling(char *remote_id, char *msg, uint32_t msglen)
{
    cJSON *jsignaling = NULL;
    cJSON *jheader = NULL;
    cJSON *jsession_id = NULL;
    cJSON *jremote_id = NULL;
    cJSON *jtype = NULL;
    cJSON *jpath = NULL;
    char *path = "mqtt";
    jsignaling = cJSON_Parse(msg);
    if (!cJSON_IsObject(jsignaling)) {
        printf("set signaling: not a json(%.*s)\n", msglen, msg);
        goto finish;
    }
    jheader = cJSON_GetObjectItemCaseSensitive(jsignaling, "header");
    if (!cJSON_IsObject(jsignaling)) {
        printf("set signaling: invalid json\n");
        goto finish;
    }
    jsession_id = cJSON_GetObjectItemCaseSensitive(jheader, "sessionid");
    if (!cJSON_IsString(jsession_id)) {
        printf("set signaling: invalid json\n");
        goto finish;
    }
    jremote_id = cJSON_GetObjectItemCaseSensitive(jheader, "from");
    if (!cJSON_IsString(jremote_id)) {
        printf("set signaling: invalid json\n");
        goto finish;
    }
    jtype = cJSON_GetObjectItemCaseSensitive(jheader, "type");
    if (!cJSON_IsString(jtype)) {
        printf("set signaling: invalid json\n");
        goto finish;
    }
    jpath = cJSON_GetObjectItemCaseSensitive(jheader, "path");
    if (cJSON_IsString(jpath)) {
        path = jpath->valuestring;
    }
    if (strcmp(jtype->valuestring, "offer") == 0) {
        // int ret = ctx_add_session(g_ctx, jsession_id->valuestring);
        // if (ret < 0) {
        //     tuya_p2p_log_info("got repeated offer, drop\n");
        //     char control_msg[1024] = {0};
        //     snprintf(control_msg,
        //              sizeof(control_msg),
        //              "{\"cmd\":\"retransmit_signaling\",\"args\":{\"session_id\":\"%s\", "
        //              "\"remote_id\":\"%s\",\"path\":\"%s\"}}",
        //              jsession_id->valuestring,
        //              jremote_id->valuestring,
        //              path);
        //     ctx_input_control_msg(control_msg, strlen(control_msg));
        //     goto finish;
        // }
    }

    // Regardless of whether the received type field is "offer" or "candidate", process with the following function
    int ret = tuya_p2p_process_signal_msg(msg, msglen);
    if (ret < 0) {
        goto finish;
    }

finish:
    if (jsignaling != NULL) {
        cJSON_Delete(jsignaling);
    }
    return 0;
}

int ctx_session_add_remote_candidate(tuya_p2p_rtc_session_t *rtc, rtc_sdp_t *remote_sdp, char *candidate)
{
    pj_ice_session_t *pIceSession = rtc->pIce;
    int iCandidateLen = strlen(candidate);
    if (iCandidateLen == 0) {
        return -1;
    }
    tuya_p2p_rtc_sdp_add_candidate(remote_sdp, candidate);
    pj_str_t pjstrCand = pj_str(candidate);
    if (*(candidate + iCandidateLen - 2) == '\r' && *(candidate + iCandidateLen - 1) == '\n') {
        pjstrCand.slen -= 2; // Remove trailing \r\n character length
    }
    pj_ice_sess_cand cand;
    if (parse_cand(NULL, &pjstrCand, &cand) !=
        0 /*|| cand.type == PJ_ICE_CAND_TYPE_HOST || cand.type == PJ_ICE_CAND_TYPE_RELAYED*/) {
        return -1;
    }
    pj_str_t pjstrUFrag = pj_str(remote_sdp->ufrag);
    pj_str_t pjstrPasswd = pj_str(remote_sdp->password);
    pj_ice_session_add_remote_candidate(pIceSession, &pjstrUFrag, &pjstrPasswd, 1, &cand, false);
    return 0;
}

int ctx_session_send_sdp(tuya_p2p_rtc_session_t *rtc, rtc_session_cfg_t *cfg)
{
    char *type = (cfg->role == PJ_ROLE_CALLER ? "offer" : "answer");
    char sdp[4 * 1024] = {0};
    int ret = tuya_p2p_rtc_sdp_encode(&rtc->local_sdp, type, sdp, sizeof(sdp));
    if (ret < 0) {
        return -1;
    }

    char *signaling = NULL;
    cJSON *jsignaling = NULL;

    cJSON *jfrom = cJSON_CreateString(cfg->local_id);
    cJSON *jto = cJSON_CreateString(cfg->remote_id);
    cJSON *jsession_id = cJSON_CreateString(cfg->session_id);
    cJSON *jmoto_id = cJSON_CreateString(cfg->moto_id);
    cJSON *jtype = cJSON_CreateString(type);
    cJSON *jtrace_id = cJSON_CreateString(cfg->trace_id);
    cJSON *jis_pre = cJSON_CreateNumber(cfg->is_pre);
    cJSON *jsecurity_level = cJSON_CreateNumber(cfg->security_level);
    cJSON *jp2p_skill = cJSON_CreateNumber(g_uP2PSkill);
    cJSON *jheader = cJSON_CreateObject();
    if (jfrom == NULL || jto == NULL || jsession_id == NULL || jmoto_id == NULL || jtype == NULL || jtrace_id == NULL ||
        jis_pre == NULL || jp2p_skill == NULL || jheader == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jheader, "from", jfrom);
    cJSON_AddItemToObject(jheader, "to", jto);
    cJSON_AddItemToObject(jheader, "sessionid", jsession_id);
    cJSON_AddItemToObject(jheader, "moto_id", jmoto_id);
    cJSON_AddItemToObject(jheader, "type", jtype);
    cJSON_AddItemToObject(jheader, "trace_id", jtrace_id);
    cJSON_AddItemToObject(jheader, "is_pre", jis_pre);
    cJSON_AddItemToObject(jheader, "p2p_skill", jp2p_skill);
    cJSON_AddItemToObject(jheader, "security_level", jsecurity_level);

    cJSON *jsdp = cJSON_CreateString(sdp);
    cJSON *jpreconnect = cJSON_CreateBool(cfg->preconnect_enable);
    cJSON *jtoken = cJSON_Parse(cfg->ice_server_tokens);
    cJSON *judp_token = cJSON_Parse(cfg->udp_server_tokens);
    cJSON *jtcp_token = cJSON_Parse(cfg->tcp_server_tokens);
    cJSON *jmsg = cJSON_CreateObject();
    if (jsdp == NULL || jmsg == NULL || jpreconnect == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jmsg, "sdp", jsdp);
    // cJSON_AddItemToObject(jmsg, "preconnect", jpreconnect);
    if (jtoken != NULL) {
        cJSON_AddItemToObject(jmsg, "token", jtoken);
    }
    if (judp_token != NULL) {
        cJSON_AddItemToObject(jmsg, "udp_token", judp_token);
    }
    if (jtcp_token != NULL) {
        cJSON_AddItemToObject(jmsg, "tcp_token", jtcp_token);
    }

    jsignaling = cJSON_CreateObject();
    if (jsignaling == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jsignaling, "header", jheader);
    cJSON_AddItemToObject(jsignaling, "msg", jmsg);

    signaling = cJSON_PrintUnformatted(jsignaling);
    if (signaling == NULL) {
        goto finish;
    }
    // ctx_session_send_signaling(rtc, signaling, 0);
    // ctx_session_backup_signaling(rtc, "outgoing", type, signaling);
    tal_mutex_lock(g_p2p_session_mutex);
    if (g_pRtcSession->cb.on_signaling != NULL) {
        g_pRtcSession->cb.on_signaling(cfg->remote_id, signaling, strlen(signaling));
        printf("g_pRtcSession->cb.on_signaling success\n");
    }
    tal_mutex_unlock(g_p2p_session_mutex);

finish:
    if (signaling != NULL) {
        cJSON_free(signaling);
    }
    if (jsignaling != NULL) {
        cJSON_Delete(jsignaling);
    }
    return 0;
}

int ctx_session_send_candidate(tuya_p2p_rtc_session_t *rtc, rtc_session_cfg_t *cfg, char *cand_str)
{
    char *type = "candidate";
    char *signaling = NULL;
    cJSON *jsignaling = NULL;

    cJSON *jfrom = cJSON_CreateString(cfg->local_id);
    cJSON *jto = cJSON_CreateString(cfg->remote_id);
    cJSON *jsession_id = cJSON_CreateString(cfg->session_id);
    cJSON *jmoto_id = cJSON_CreateString(cfg->moto_id);
    cJSON *jtype = cJSON_CreateString(type);
    cJSON *jtrace_id = cJSON_CreateString(cfg->trace_id);
    cJSON *jheader = cJSON_CreateObject();
    if (jfrom == NULL || jto == NULL || jsession_id == NULL || jmoto_id == NULL || jtype == NULL || jtrace_id == NULL ||
        jheader == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jheader, "from", jfrom);
    cJSON_AddItemToObject(jheader, "to", jto);
    cJSON_AddItemToObject(jheader, "sessionid", jsession_id);
    cJSON_AddItemToObject(jheader, "moto_id", jmoto_id);
    cJSON_AddItemToObject(jheader, "type", jtype);
    cJSON_AddItemToObject(jheader, "trace_id", jtrace_id);

    cJSON *jcand = cJSON_CreateString(cand_str);
    cJSON *jmsg = cJSON_CreateObject();
    if (jcand == NULL || jmsg == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jmsg, "candidate", jcand);

    jsignaling = cJSON_CreateObject();
    if (jsignaling == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jsignaling, "header", jheader);
    cJSON_AddItemToObject(jsignaling, "msg", jmsg);

    signaling = cJSON_PrintUnformatted(jsignaling);
    if (signaling == NULL) {
        goto finish;
    }
    // ctx_session_send_signaling(rtc, signaling, 0);
    // ctx_session_backup_signaling(rtc, "outgoing", type, signaling);
    tal_mutex_lock(g_p2p_session_mutex);
    if (g_pRtcSession->cb.on_signaling != NULL) {
        g_pRtcSession->cb.on_signaling(cfg->remote_id, signaling, strlen(signaling));
    }
    tal_mutex_unlock(g_p2p_session_mutex);

finish:
    if (signaling != NULL) {
        cJSON_free(signaling);
    }
    if (jsignaling != NULL) {
        cJSON_Delete(jsignaling);
    }
    return 0;
}

int ctx_session_send_suspend_resp(tuya_p2p_rtc_session_t *rtc, int error)
{
    char *type = "suspend_resp";
    char *signaling = NULL;
    cJSON *jsignaling = NULL;

    cJSON *jfrom = cJSON_CreateString(rtc->cfg.local_id);
    cJSON *jto = cJSON_CreateString(rtc->cfg.remote_id);
    cJSON *jsession_id = cJSON_CreateString(rtc->cfg.session_id);
    cJSON *jmoto_id = cJSON_CreateString(rtc->cfg.moto_id);
    cJSON *jtype = cJSON_CreateString(type);
    cJSON *jtrace_id = cJSON_CreateString(rtc->cfg.trace_id);
    cJSON *jheader = cJSON_CreateObject();
    if (jfrom == NULL || jto == NULL || jsession_id == NULL || jmoto_id == NULL || jtype == NULL || jtrace_id == NULL ||
        jheader == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jheader, "from", jfrom);
    cJSON_AddItemToObject(jheader, "to", jto);
    cJSON_AddItemToObject(jheader, "sessionid", jsession_id);
    cJSON_AddItemToObject(jheader, "moto_id", jmoto_id);
    cJSON_AddItemToObject(jheader, "type", jtype);
    cJSON_AddItemToObject(jheader, "trace_id", jtrace_id);

    cJSON *jhandle = cJSON_CreateNumber(rtc->active_handle);
    cJSON *jerror = cJSON_CreateNumber(error);
    cJSON *jseq = cJSON_CreateNumber(++rtc->local_cmd_seq);
    cJSON *jmsg = cJSON_CreateObject();
    if (jhandle == NULL || jerror == NULL || jseq == NULL || jmsg == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jmsg, "handle", jhandle);
    cJSON_AddItemToObject(jmsg, "error", jerror);
    cJSON_AddItemToObject(jmsg, "seq", jseq);

    jsignaling = cJSON_CreateObject();
    if (jsignaling == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jsignaling, "header", jheader);
    cJSON_AddItemToObject(jsignaling, "msg", jmsg);

    signaling = cJSON_PrintUnformatted(jsignaling);
    if (signaling == NULL) {
        goto finish;
    }
    // ctx_session_send_signaling(rtc, signaling, 0);
    if (rtc->cb.on_signaling != NULL) {
        rtc->cb.on_signaling(rtc->cfg.remote_id, signaling, strlen(signaling));
        printf("rtc->cb.on_signaling success\n");
    }

finish:
    if (signaling != NULL) {
        cJSON_free(signaling);
    }
    if (jsignaling != NULL) {
        cJSON_Delete(jsignaling);
    }
    return 0;
}

int ctx_session_send_disconnect(tuya_p2p_rtc_session_t *rtc, int32_t close_reason_local, rtc_session_close_reason_e close_reason)
{
    char *type = "disconnect";
    char *signaling = NULL;
    cJSON *jsignaling = NULL;

    cJSON *jfrom = cJSON_CreateString(rtc->cfg.local_id);
    cJSON *jto = cJSON_CreateString(rtc->cfg.remote_id);
    cJSON *jnode_id = cJSON_CreateString(rtc->cfg.node_id);
    cJSON *jsession_id = cJSON_CreateString(rtc->cfg.session_id);
    cJSON *jmoto_id = cJSON_CreateString(rtc->cfg.moto_id);
    cJSON *jtype = cJSON_CreateString(type);
    cJSON *jtrace_id = cJSON_CreateString(rtc->cfg.trace_id);
    cJSON *jclose_reason_local = cJSON_CreateNumber(close_reason_local);
    cJSON *jclose_reason = cJSON_CreateNumber(close_reason);
    cJSON *jheader = cJSON_CreateObject();
    if (jfrom == NULL || jto == NULL || jsession_id == NULL || jnode_id == NULL || jmoto_id == NULL ||
        jtype == NULL || jtrace_id == NULL || jheader == NULL || jclose_reason_local == NULL ||
        jclose_reason == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jheader, "from", jfrom);
    cJSON_AddItemToObject(jheader, "to", jto);
    cJSON_AddItemToObject(jheader, "sub_dev_id", jnode_id);
    cJSON_AddItemToObject(jheader, "sessionid", jsession_id);
    cJSON_AddItemToObject(jheader, "moto_id", jmoto_id);
    cJSON_AddItemToObject(jheader, "type", jtype);
    cJSON_AddItemToObject(jheader, "trace_id", jtrace_id);

    cJSON *jmsg = cJSON_CreateObject();
    if (jmsg == NULL) {
        goto finish;
    }

    jsignaling = cJSON_CreateObject();
    if (jsignaling == NULL) {
        goto finish;
    }
    cJSON_AddItemToObject(jsignaling, "header", jheader);
    cJSON_AddItemToObject(jmsg, "close_reason_local", jclose_reason_local);
    cJSON_AddItemToObject(jmsg, "close_reason", jclose_reason);
    cJSON_AddItemToObject(jsignaling, "msg", jmsg);

    signaling = cJSON_PrintUnformatted(jsignaling);
    if (signaling == NULL) {
        goto finish;
    }
    ctx_session_send_signaling(rtc, signaling);

finish:
    if (signaling != NULL) {
        cJSON_free(signaling);
    }
    if (jsignaling != NULL) {
        cJSON_Delete(jsignaling);
    }
    return 0;
}

int ctx_session_send_signaling(tuya_p2p_rtc_session_t *rtc, char *signaling)
{
    if (g_pRtcSession->cb.on_signaling != NULL) {
        g_pRtcSession->cb.on_signaling(rtc->cfg.remote_id, signaling, strlen(signaling));
    }
    return 0;
}

char *ctx_signaling_add_path(char *signaling, char *path) {
    char *new_signaling = NULL;
    cJSON *jsignaling = cJSON_Parse(signaling);
    if (!cJSON_IsObject(jsignaling)) {
        goto finish;
    }

    cJSON *jheader = cJSON_GetObjectItemCaseSensitive(jsignaling, "header");
    if (!cJSON_IsObject(jheader)) {
        goto finish;
    }

    cJSON *jpath = cJSON_CreateString(path);
    if (jpath == NULL) {
        goto finish;
    }

    cJSON_AddItemToObject(jheader, "path", jpath);
    new_signaling = cJSON_PrintUnformatted(jsignaling);

finish:
    if (jsignaling != NULL) {
        cJSON_Delete(jsignaling);
    }
    return new_signaling;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ice_on_ice_complete(pj_ice_strans *ice_st, pj_ice_strans_op op, pj_status_t status)
{
    tuya_p2p_rtc_session_t *pRtcSession = (tuya_p2p_rtc_session_t *)pj_ice_strans_get_user_data(ice_st);
    pj_ice_session_t *pIceSession = (pj_ice_session_t *)pRtcSession->pIce;
    if (!pIceSession)
        return;

    switch (op) {
    case PJ_ICE_STRANS_OP_INIT: {
        break;
    }
    case PJ_ICE_STRANS_OP_NEGOTIATION: {
        if (status == PJ_SUCCESS) {
            char szLCandAddr[PJ_INET6_ADDRSTRLEN + 10] = {0};
            char szRCandAddr[PJ_INET6_ADDRSTRLEN + 10] = {0};
            unsigned comp_id = 1; // Component starting ID number is 1
            const pj_ice_sess_check *pIceSessCheck = pj_ice_strans_get_valid_pair(ice_st, comp_id);
            pj_sockaddr_print(&pIceSessCheck->lcand->addr, szLCandAddr, sizeof(szLCandAddr), 3);
            pj_sockaddr_print(&pIceSessCheck->rcand->addr, szRCandAddr, sizeof(szRCandAddr), 3);

            rtc_init_mbedtls_md_and_aes(pRtcSession);
            sync_cond_notify(&g_syncCond);
            printf("ICE STrans negotiation success!\n");
        } else {
            printf("ICE STrans negotiation fail!\n");
        }
        break;
    }
    case PJ_ICE_STRANS_OP_KEEP_ALIVE: {
        break;
    }
    default: {
        pj_assert(!"Unknown op");
        break;
    }
    } // switch (op)

    return;
}

void ice_on_new_candidate(pj_ice_strans *ice_st, const pj_ice_sess_cand *cand, pj_bool_t last)
{
    tuya_p2p_rtc_session_t *pRtcSession = (tuya_p2p_rtc_session_t *)pj_ice_strans_get_user_data(ice_st);
    pj_ice_session_t *pIceSession = (pj_ice_session_t *)pRtcSession->pIce;
    if (!pIceSession)
        return;

    // pIceSession->bLastCand = last;

    if (cand) {
        char buf1[PJ_INET6_ADDRSTRLEN + 10] = {0};
        char buf2[PJ_INET6_ADDRSTRLEN + 10] = {0};
        PJ_LOG(4, (THIS_FILE,
                   "%p: discovered a new candidate "
                   "comp=%d, type=%s, addr=%s, baseaddr=%s, end=%d",
                   pIceSession, cand->comp_id, pj_ice_get_cand_type_name(cand->type),
                   pj_sockaddr_print(&cand->addr, buf1, sizeof(buf1), 3),
                   pj_sockaddr_print(&cand->base_addr, buf2, sizeof(buf2), 3), last));
        char szCand[1024] = {0};
        if (print_cand(szCand, sizeof(szCand), cand) < 0) {
            return;
        }
        ctx_session_send_candidate(pRtcSession, &cfg, szCand);
    }
    // else if (pIceSession->pIceSTransport && last) {
    // 	PJ_LOG(4, (THIS_FILE, "%p: end of candidate", pIceSession->pIceSTransport));
    // }

    return;
}

void ice_on_rx_data(pj_ice_strans *ice_st, unsigned comp_id, void *buffer, pj_size_t size,
                    const pj_sockaddr_t *src_addr, unsigned src_addr_len)
{
    tuya_p2p_rtc_session_t *rtc = (tuya_p2p_rtc_session_t *)pj_ice_strans_get_user_data(ice_st);
    if (rtc == NULL) {
        return;
    }
    tuya_uv_buf_t pkt;
    pkt.base = buffer;
    pkt.len = size;
    rtc_process_kcp_data(rtc, &pkt);
    return;
}

void rtc_process_kcp_data(tuya_p2p_rtc_session_t *rtc, const tuya_uv_buf_t *pkt)
{
    if (rtc == NULL || pkt == NULL) {
        return;
    }
    uint32_t digest_len = 0;
    if (rtc->cfg.security_level == TUYA_P2P_SECURITY_LEVEL_3) {
        digest_len = mbedtls_md_get_size(rtc->md_info);
    }
    if (pkt->len < IKCP_PACKET_HEADER_SIZE + digest_len) {
        tuya_p2p_log_debug("recv invalid packet, len = %d\n", pkt->len);
        return;
    }
    uint32_t channel_id = ikcp_getconv(pkt->base);
    if (channel_id < 0 || channel_id > rtc->cfg.channel_number) {
        tuya_p2p_log_warn("recv invalid kcp packet, channel id = %d(%d)\n", channel_id, rtc->cfg.channel_number);
        return;
    }
    rtc_channel_t *chan = &rtc->channels[channel_id];
    chan->socket_recv_bytes += (pkt->len);

    if (digest_len > 0) {
        unsigned char digest[digest_len];
        int ret;
        ret = mbedtls_md_hmac_starts(&rtc->md_ctx, rtc->aes_key, sizeof(rtc->aes_key));
        if (ret != 0) {
            return;
        }
        ret = mbedtls_md_hmac_update(&rtc->md_ctx, (unsigned char *)pkt->base, pkt->len - digest_len);
        if (ret != 0) {
            return;
        }
        ret = mbedtls_md_hmac_finish(&rtc->md_ctx, digest);
        if (ret != 0) {
            return;
        }

        if (memcmp(digest, pkt->base + pkt->len - digest_len, digest_len)) {
            tuya_p2p_log_debug("invalid md code\n");
            return;
        }
    }

    ctx_session_channel_process_data(chan, pkt->base, pkt->len - digest_len);

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

static int on_kcp_output(const char *buf, int len, ikcpcb *kcp, void *user_data)
{
    (void)kcp;
    if (user_data == NULL) {
        return 0;
    }
    rtc_channel_t *chan = (rtc_channel_t *)user_data;
    tuya_p2p_rtc_session_t *rtc = chan->rtc;

    int md_size = 0;
    if (rtc->cfg.security_level == TUYA_P2P_SECURITY_LEVEL_3) {
        int ret;
        ret = mbedtls_md_hmac_starts(&rtc->md_ctx, rtc->aes_key, sizeof(rtc->aes_key));
        if (ret != 0) {
            return 0;
        }
        ret = mbedtls_md_hmac_update(&rtc->md_ctx, (unsigned char *)buf, len);
        if (ret != 0) {
            return 0;
        }
        ret = mbedtls_md_hmac_finish(&rtc->md_ctx, (unsigned char *)buf + len);
        if (ret != 0) {
            return 0;
        }
        md_size = mbedtls_md_get_size(rtc->md_info);
    }

    ctx_session_channel_set_send_time(chan);
    uint32_t r = (rand() % 99) + 1;
    uint32_t channel_id = ikcp_getconv(buf);
    unsigned char cmd = ikcp_getcmd(buf);
    uint32_t sn = ikcp_getsn(buf);
    // tuya_p2p_log_trace("channel_id: %08x, sn: %d, cmd: %d\n", channel_id, sn, cmd);

    if (cmd != KCP_CMD_PUSH || channel_id != RTC_CHANNEL_CMD) {
        pj_ice_session_sendto(rtc->pIce, (void *)buf, len + md_size);
    }

    chan->socket_send_bytes += (len + md_size);
    return len;
}

void rtc_ref_cnt_add(tuya_p2p_rtc_session_t *rtc) {
    pthread_mutex_lock(&rtc->ref_lock);
    rtc->ref_cnt++;
    pthread_mutex_unlock(&rtc->ref_lock);
}

void rtc_ref_cnt_del(tuya_p2p_rtc_session_t *rtc) {
    pthread_mutex_lock(&rtc->ref_lock);
    rtc->ref_cnt--;
    pthread_mutex_unlock(&rtc->ref_lock);
}

int rtc_ref_cnt_get(tuya_p2p_rtc_session_t *rtc) {
    int ref_cnt;
    pthread_mutex_lock(&rtc->ref_lock);
    ref_cnt = rtc->ref_cnt;
    pthread_mutex_unlock(&rtc->ref_lock);
    return ref_cnt;
}

tuya_p2p_rtc_session_t *ctx_session_create(rtc_session_cfg_t *cfg, rtc_state_e state, int32_t *err_code)
{
    tuya_p2p_rtc_session_t *rtc = NULL;
    rtc = (tuya_p2p_rtc_session_t *)malloc(sizeof(tuya_p2p_rtc_session_t));
    if (rtc == NULL) {
        *err_code = TUYA_P2P_ERROR_OUT_OF_MEMORY;
        goto finish;
    }
    memset(rtc, 0, sizeof(tuya_p2p_rtc_session_t));
    memcpy(&rtc->cfg, cfg, sizeof(rtc->cfg));
    // rtc->pool = NULL;
    rtc->ref_cnt = 0;
    pthread_mutex_init(&rtc->ref_lock, NULL);
    pthread_mutex_init(&rtc->channel_lock, NULL);
    rtc->cfg.channel_number = 3;
    rtc->active_handle = 0;
    rtc->local_cmd_seq = 0;
    rtc->tid = -1;
    rtc->bQuitKCPThread = false;

    sync_cond_init(&rtc->syncCondExit);

    if (tuya_p2p_rtc_channels_init(rtc) != 0) {
        *err_code = TUYA_P2P_ERROR_CHANNEL_INIT_FAILED;
        goto finish;
    }

    int ret = 0;
    tuya_p2p_rtc_dtls_role_e local_dtls_role =
        DTLS_ROLE_CLIENT /*rtc->cfg.role == RTC_ROLE_CALLER ? DTLS_ROLE_BOTH : DTLS_ROLE_CLIENT*/;
    tuya_p2p_rtc_dtls_role_e remote_dtls_role =
        DTLS_ROLE_SERVER /*rtc->cfg.role == RTC_ROLE_CALLER ? DTLS_ROLE_BOTH : DTLS_ROLE_SERVER*/;
    ret = tuya_p2p_rtc_sdp_init(&rtc->local_sdp, cfg->session_id, cfg->local_id, "", cfg->ice_ufrag, cfg->ice_password,
                                local_dtls_role);
    if (ret < 0) {
        *err_code = TUYA_P2P_ERROR_SDP_INIT_FAILED;
        goto finish;
    }
    ret = tuya_p2p_rtc_sdp_init(&rtc->remote_sdp, "", "", "", NULL, NULL, remote_dtls_role);
    if (ret < 0) {
        *err_code = TUYA_P2P_ERROR_SDP_INIT_FAILED;
        goto finish;
    }
    rtc_ref_cnt_add(rtc);
    return rtc;

finish:
    if (rtc != NULL) {
        ctx_session_destroy(rtc);
    }
    return NULL;
}

void ctx_session_destroy(tuya_p2p_rtc_session_t *rtc)
{
    tal_mutex_lock(g_p2p_session_mutex);
    if (rtc == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        return;
    }
    if (rtc->tid != -1) {
        rtc->bQuitKCPThread = true;
        pthread_join(rtc->tid, NULL);
        rtc->tid = -1;
    }
    tuya_p2p_rtc_channels_destroy(rtc);
    if (rtc->pIce != NULL) {
        pj_ice_session_destroy(rtc->pIce);
        rtc->pIce = NULL;
    }
    tal_mutex_unlock(g_p2p_session_mutex);

    sync_cond_wait(&rtc->syncCondExit);
    sync_cond_clean(&rtc->syncCondExit);

    tal_mutex_lock(g_p2p_session_mutex);
    tuya_p2p_rtc_sdp_deinit(&rtc->local_sdp);
    tuya_p2p_rtc_sdp_deinit(&rtc->remote_sdp);
    mbedtls_md_free(&rtc->md_ctx);
    pthread_mutex_destroy(&rtc->ref_lock);
    pthread_mutex_destroy(&rtc->channel_lock);
    free(rtc);
    rtc = NULL;
    tal_mutex_unlock(g_p2p_session_mutex);
    return;
}

void ctx_session_channel_set_data_time(struct rtc_channel *chan)
{
    if (chan->first_data_time_ms == 0) {
        tuya_p2p_log_warn("channel %d get first data\n", chan->channel_id);
        chan->first_data_time_ms = tuya_p2p_misc_get_timestamp_ms();
    }
}

void ctx_session_channel_set_write_time(struct rtc_channel *chan)
{
    if (chan->first_write_time_ms == 0) {
        tuya_p2p_log_warn("channel %d first write\n", chan->channel_id);
        chan->first_write_time_ms = tuya_p2p_misc_get_timestamp_ms();
    }
}

void ctx_session_channel_set_send_time(struct rtc_channel *chan)
{
    if (chan->first_send_time_ms == 0) {
        tuya_p2p_log_warn("channel %d first send\n", chan->channel_id);
        chan->first_send_time_ms = tuya_p2p_misc_get_timestamp_ms();
    }
}

int ctx_session_channel_process_data(struct rtc_channel *chan, char *data, int len)
{
    ctx_session_channel_set_data_time(chan);
    if (ikcp_input(chan->kcp, (const char *)data, len /*, tuya_p2p_misc_get_timestamp_ms()*/) > 0) {
    }
    return 0;
}

int ctx_session_channel_process_pkt(void *user, int length, const char *input, char *output)
{
    char *encrypted = (char *)input;
    char *decrypted = output;
    int iv_size = 16;
    int keylen = 16;
    char *iv = encrypted;
    int msg_size = length - iv_size;
    rtc_channel_t *chan = (rtc_channel_t *)user;
    tuya_p2p_rtc_session_t *rtc = chan->rtc;
    int ret = -1;
    if ((msg_size > 0) && ((msg_size % keylen) == 0)) {
        ret = rtc_crypt_decrypt_aes_128_cbc(rtc, chan->aes_ctx_dec, msg_size, (unsigned char *)iv,
                                            (const unsigned char *)(encrypted + iv_size), (unsigned char *)decrypted);

        // Subtract GCM signature length
        if (rtc->cfg.security_level == TUYA_P2P_SECURITY_LEVEL_4) {
            if (msg_size <= 16) {
                return -1;
            }
            msg_size -= 16;
        }

        if (ret == 0) {
            int padding_size = decrypted[msg_size - 1];
            if (padding_size <= 16) {
                if (padding_size < msg_size) {
                    ret = msg_size - padding_size;
                }
            }
        }
    }

    return ret;
}

int tuya_p2p_rtc_channels_init(tuya_p2p_rtc_session_t *rtc)
{
    uint32_t i = 0;
    // tuya_mem_pool_t *pool = tuya_mem_pool_create(TUYA_MBUF_HUGE_SIZE, TUYA_MBUF_NUM_ONCE);
    // if (NULL == pool) {
    //     goto finish;
    // }
    // rtc->pool = pool;
    rtc->channels = (rtc_channel_t *)malloc((rtc->cfg.channel_number + 1) * sizeof(rtc_channel_t));
    if (rtc->channels == NULL) {
        goto finish;
    }
    for (i = 0; i < rtc->cfg.channel_number + 1; i++) {
        uint32_t channel_id;
        uint32_t send_buf_size;
        uint32_t recv_buf_size;
        if (i == rtc->cfg.channel_number) {
            channel_id = RTC_CHANNEL_CMD;
            send_buf_size = 100 * 1024;
            recv_buf_size = 100 * 1024;
        } else {
            channel_id = i;
            send_buf_size = g_options.send_buf_size[i];
            recv_buf_size = g_options.recv_buf_size[i];
            // if (rtc->cfg.is_pre) {
            //     channel_id |= (rtc->active_handle << 16) & 0xFFFF0000;
            // }
        }
        rtc_channel_t *chan = &rtc->channels[i];
        memset(chan, 0, sizeof(*chan));
        chan->rtc = rtc;
        chan->channel_id = i;
        // chan->send_queue = tuya_mbuf_queue_create(send_buf_size, pool);
        // chan->recv_queue = tuya_mbuf_queue_create(recv_buf_size, pool);
        chan->kcp = ikcp_create(channel_id, chan /*, chan->send_queue, chan->recv_queue*/);
        if (chan->kcp == NULL /*|| chan->send_queue == NULL || chan->recv_queue == NULL*/) {
            goto finish;
        }

        ikcp_setoutput(chan->kcp, on_kcp_output);
        ikcp_wndsize(chan->kcp, send_buf_size / 1600  /*TUYA_MBUF_HUGE_SIZE*/,
                     recv_buf_size / 1600 /*TUYA_MBUF_HUGE_SIZE*/);
        ikcp_nodelay(chan->kcp, 0, 10, 20, 1);
        ikcp_setmtu(chan->kcp, 1400);
        ikcp_setprocesspkt(chan->kcp, ctx_session_channel_process_pkt);
        // ikcp_setwritelog(chan->kcp, ctx_session_kcp_writelog);
        // ikcp_setlogmask(chan->kcp, IKCP_LOG_RTT | IKCP_LOG_INPUT | IKCP_LOG_OUTPUT);
        // ikcp_setlogmask(chan->kcp, IKCP_LOG_RECV);
        // ikcp_setlogmask(chan->kcp, IKCP_LOG_IN_DROP | IKCP_LOG_IN_DATA);
        // ikcp_setlogmask(chan->kcp, IKCP_LOG_OUT_DATA | IKCP_LOG_IN_ACK|IKCP_LOG_RATE);
        // ikcp_setlogmask(chan->kcp, 0xffffffff);
    }
    return 0;
finish:
    return -1;
}

void tuya_p2p_rtc_channels_destroy(tuya_p2p_rtc_session_t *rtc)
{
    if (rtc->channels != NULL) {
        uint32_t i;
        for (i = 0; i < rtc->cfg.channel_number + 1; i++) {
            rtc_channel_t *chan = &rtc->channels[i];
            if (chan->kcp != NULL) {
                ikcp_release(chan->kcp);
                chan->kcp = NULL;
            }
            rtc_channel_aes_uninit(chan);
            // if (chan->send_queue != NULL) {
            //     tuya_mbuf_queue_destroy(chan->send_queue);
            // }
            // if (chan->recv_queue != NULL) {
            //     tuya_mbuf_queue_destroy(chan->recv_queue);
            // }
            // g_crypt_table[rtc->cfg.security_level].uninit(chan);
        }
        free(rtc->channels);
        rtc->channels = NULL;
    }

    // if (NULL != rtc->pool) {
    //     tuya_mem_pool_destroy(rtc->pool);
    //     rtc->pool = NULL;
    // }
    return;
}

void *rtc_worker_thread(void *arg)
{
    // Execute KCP sending and receiving in the same thread
    pj_thread_register2();
    tuya_p2p_rtc_session_t *rtc = (tuya_p2p_rtc_session_t *)arg;
    while (!rtc->bQuitKCPThread) {
        pj_ice_session_handle_events(rtc->pIce, 5, NULL); // Drive ICE state update and execute KCP receive operation
        for (int i = 0; i < 3; ++i)                        //(rtc->cfg.channel_number + 1)
        {
            rtc_channel_t *channel = &rtc->channels[i];
            ikcp_update(channel->kcp,
                        tuya_p2p_misc_get_timestamp_ms()); // Drive KCP state update and execute KCP send operation
        }
    }
    return NULL;
}

int32_t tuya_p2p_rtc_listen()
{
    sync_cond_wait(&g_syncCond);
    return 123456; // Temporarily return a random integer value, to be changed later
}

int32_t tuya_p2p_rtc_dosend_data(tuya_p2p_rtc_session_t *rtc, uint32_t channel_id, char *buf, int32_t len,
                                 int32_t timeout_ms)
{
    if (rtc == NULL) {
        return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
    }
    int remain = len;
    int already = 0;
    int rc = 0;
    uint64_t begin_time = 0;

    while (remain > 0) {
        pthread_mutex_lock(&rtc->channel_lock);
        if (rtc->channels == NULL) {
            pthread_mutex_unlock(&rtc->channel_lock);
            rc = -1;
            break;
        }
        if (timeout_ms > 0) {
            if (begin_time == 0) {
                begin_time = tuya_p2p_misc_get_timestamp_ms();
            }
            if (tuya_p2p_misc_check_timeout(begin_time, timeout_ms)) {
                pthread_mutex_unlock(&rtc->channel_lock);
                tuya_p2p_log_error("tuya_p2p_misc_check_timeout");
                return TUYA_P2P_ERROR_TIME_OUT;
            }
        }
        rtc_channel_t *chan = &rtc->channels[channel_id];
        ctx_session_channel_set_write_time(chan);
        // if (0 != tuya_mbuf_queue_get_status(chan->send_queue)) {
        //     //printf("rtc %08x channel %d over\n", rtc->handle, channel_id);
        //     pthread_mutex_unlock(&rtc->channel_lock);
        //     rc = -1;
        //     break;
        // }
        // if (tuya_mbuf_queue_get_free_size(chan->send_queue) <= 0) {
        //     if (timeout_ms == 0) {
        //         pthread_mutex_unlock(&rtc->channel_lock);
        //         break;
        //     }
        //     pthread_mutex_unlock(&rtc->channel_lock);
        //     usleep(5 * 1000);
        //     continue;
        // }

        int fragement_len = 1200;
        int current = (remain > fragement_len) ? (fragement_len) : remain;
        char decrypted[1500];
        char *encrypted;
        int iv_size = sizeof(rtc->iv);
        int keylen = 16;
        int sign_size = 0;
        unsigned char padding_size = keylen;
        int reserve_len_forkcp = sizeof(struct IKCPSEG) + IKCP_PACKET_HEADER_SIZE;
        /* 60 bytes prepared for signature */
        int reserve_len = sizeof(struct IKCPSEG) + IKCP_PACKET_HEADER_SIZE + sizeof(rtc->iv) + 60;
        int buflen;

        buflen = current;
        memcpy(decrypted, buf + already, buflen);

        padding_size -= (buflen % keylen);
        memset(&(decrypted[buflen]), padding_size, padding_size);

        buflen += padding_size;

        /* Reserved length for control header and kcp packet header needed for kcp chaining */
        // tuya_mbuf_t *mbuf_encrypted = tuya_mbuf_alloc(chan->send_queue, buflen + reserve_len, reserve_len_forkcp);
        // if (NULL == mbuf_encrypted) {
        //     pthread_mutex_unlock(&rtc->channel_lock);
        //     /* This step will not fail unless out of memory, this step failure also means connection abnormal,
        //     because the packet just taken down has not been added to the kcp queue */ break;
        // } else {
        //     encrypted = TUYA_MBUF_MTOD(mbuf_encrypted);
        // }
        encrypted = (char *)malloc(buflen + reserve_len);
        char tmp_iv[16];
        tuya_p2p_misc_rand_hex(tmp_iv, sizeof(rtc->iv));
        memcpy(encrypted, tmp_iv, iv_size);

        int ret = rtc_crypt_encrypt_aes_128_cbc(rtc, chan->aes_ctx_enc, buflen, (unsigned char *)tmp_iv,
                                                (const unsigned char *)decrypted, (unsigned char *)encrypted + iv_size);

        // GCM encryption automatically generates 16-byte signature
        if (rtc->cfg.security_level == TUYA_P2P_SECURITY_LEVEL_4) {
            sign_size = 16;
        }

        if (ret == 0) {
            // ikcp_send_mbuf(chan->kcp, mbuf_encrypted, buflen + iv_size + sign_size);
            ikcp_send(chan->kcp, encrypted, buflen + iv_size + sign_size);
            remain -= current;
            already += current;
            chan->write_bytes += current;
            free(encrypted);
        } else {
            pthread_mutex_unlock(&rtc->channel_lock);
            tuya_p2p_log_error("aes encrypt failed, ret = %d\n", ret);
            // tuya_mbuf_free(mbuf_encrypted);
            free(encrypted);
            rc = -1;
            break;
        }
        pthread_mutex_unlock(&rtc->channel_lock);
    }

    if (already > 0) {
        tuya_p2p_log_debug("channel id %d, send rc = %d\n", channel_id, already);
        return already;
    } else if (rc < 0) {
        // tuya_p2p_log_error("rtc %08x channel %d send rc = %d\n", rtc->handle, channel_id,
        // TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT);
        return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
    } else {
        // tuya_p2p_log_debug("send rc = %d\n", TUYA_P2P_ERROR_TIME_OUT);
        return TUYA_P2P_ERROR_TIME_OUT;
    }
}

int32_t tuya_p2p_rtc_send_data(int32_t handle, uint32_t channel_id, char *buf, int32_t len, int32_t timeout_ms)
{
    tal_mutex_lock(g_p2p_session_mutex);
    tuya_p2p_rtc_session_t *rtc = g_pRtcSession;
    if (rtc == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        tuya_p2p_log_error("rtc session %08x recv data: invalid session\n", handle);
        return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    int32_t ret = tuya_p2p_rtc_dosend_data(rtc, channel_id, buf, len, timeout_ms);
    tal_mutex_unlock(g_p2p_session_mutex);
    return ret;
}

int32_t tuya_p2p_rtc_dorecv_data(tuya_p2p_rtc_session_t *rtc, uint32_t channel_id, char *buf, int32_t *len,
                                 int32_t timeout_ms)
{
    if (rtc == NULL) {
        return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
    }
    int ret = 0;
    uint64_t begin_time = 0;
    int buflen = *len;

    *len = 0;

    while (1) {
        pthread_mutex_lock(&rtc->channel_lock);
        if (rtc->channels == NULL) {
            pthread_mutex_unlock(&rtc->channel_lock);
            ret = -1;
            break;
        }
        rtc_channel_t *chan = &rtc->channels[channel_id];
        // ret = ikcp_recv_mbufwithdata(chan->kcp, buf, buflen);
        if (0 != ret) {
            chan->read_bytes += ret;
            pthread_mutex_unlock(&rtc->channel_lock);
            break;
        }
        pthread_mutex_unlock(&rtc->channel_lock);
        if (timeout_ms >= 0) {
            if (begin_time == 0) {
                begin_time = tuya_p2p_misc_get_timestamp_ms();
            }
            if (tuya_p2p_misc_check_timeout(begin_time, timeout_ms)) {
                break;
            }
        }
        usleep(10 * 1000);
    }

    if (ret < 0) {
        return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
    } else if (ret == 0) {
        return TUYA_P2P_ERROR_TIME_OUT;
    }

    *len = ret;

    return 0;
}

int32_t tuya_p2p_rtc_dorecv_data2(tuya_p2p_rtc_session_t *rtc, uint32_t channel_id, char *buf, int32_t *len,
                                  int32_t timeout_ms)
{
    int ret = 0;
    uint64_t begin_time = 0;
    int buflen = *len;
    *len = 0;

    while (1) {
        pthread_mutex_lock(&rtc->channel_lock);
        if (rtc->channels == NULL) {
            pthread_mutex_unlock(&rtc->channel_lock);
            ret = -4;
            break;
        }
        rtc_channel_t *chan = &rtc->channels[channel_id];
        ret = ikcp_recv2(chan->kcp, buf, buflen);
        if (ret > 0) {
            chan->read_bytes += ret;
            *len = ret;
            pthread_mutex_unlock(&rtc->channel_lock);
            break;
        }
        pthread_mutex_unlock(&rtc->channel_lock);
        if (timeout_ms >= 0) {
            if (begin_time == 0) {
                begin_time = tuya_p2p_misc_get_timestamp_ms();
            }
            if (tuya_p2p_misc_check_timeout(begin_time, timeout_ms)) {
                break;
            }
        }
        usleep(10 * 1000);
    }

    if (ret == -1 || ret == -2) {
        return TUYA_P2P_ERROR_TIME_OUT;
    } else if (ret == -3 || ret == -4) {
        return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
    }
    return 0;
}

int32_t tuya_p2p_rtc_recv_data(int32_t handle, uint32_t channel_id, char *buf, int32_t *len, int32_t timeout_ms)
{
    int buflen = *len;
    *len = 0;

    // if (!ctx_is_inited())
    // {
    //     tuya_p2p_log_error("rtc session %08x recv data: sdk not inited\n", handle);
    //     return TUYA_P2P_ERROR_NOT_INITIALIZED;
    // }
    // tuya_p2p_rtc_session_t *rtc = ctx_session_get_by_handle(g_ctx, handle);
    tal_mutex_lock(g_p2p_session_mutex);
    tuya_p2p_rtc_session_t *rtc = g_pRtcSession;
    if (rtc == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        tuya_p2p_log_error("rtc session %08x recv data: invalid session\n", handle);
        return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    tal_mutex_unlock(g_p2p_session_mutex);
    // int error = rtc_session_get_error(rtc);
    // if (error != TUYA_P2P_ERROR_SUCCESSFUL)
    // {
    //     ctx_session_release(g_ctx, rtc);
    //     return error;
    // }
    // if (rtc->cfg.is_webrtc) {
    //     ctx_session_release(g_ctx, rtc);
    //     tuya_p2p_log_error("rtc session %08x recv data: invalid session\n", handle);
    //     return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    // }
    if (channel_id < 0 || channel_id >= rtc->cfg.channel_number) {
        //tal_mutex_unlock(g_p2p_session_mutex);
        tuya_p2p_log_error("rtc session %08x recv data: invalid channel number: %d/%d\n", handle, channel_id,
                           rtc->cfg.channel_number);
        // ctx_session_release(g_ctx, rtc);
        return TUYA_P2P_ERROR_INVALID_PARAMETER;
    }

    *len = buflen;
    int32_t ret = tuya_p2p_rtc_dorecv_data2(rtc, channel_id, buf, len, timeout_ms);
    // rtc_channel_t *chan = &rtc->channels[channel_id];
    // ctx_session_channel_process_pkt(chan, *len, buf, buf);

    // ctx_session_release(g_ctx, rtc);
    return ret;
}

void tuya_p2p_rtc_notify_exit()
{
    //tal_mutex_lock(g_p2p_session_mutex);
    sync_cond_notify(&g_pRtcSession->syncCondExit);
    //tal_mutex_unlock(g_p2p_session_mutex);
    return;
}

int32_t tuya_p2p_rtc_check(int32_t handle)
{
    return 0;
}

int32_t tuya_p2p_rtc_check_buffer(int32_t handle, uint32_t channel_id, uint32_t *write_size, uint32_t *read_size,
                                  uint32_t *send_free_size)
{
    int ret = 0;
    tal_mutex_lock(g_p2p_session_mutex);
    if (g_pRtcSession == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    tuya_p2p_rtc_session_t *rtc = g_pRtcSession;
    pthread_mutex_lock(&rtc->channel_lock);
    if (rtc->channels != NULL) {
        rtc_channel_t *chan = &rtc->channels[channel_id];
        // if (write_size != NULL) {
        //     *write_size = tuya_mbuf_queue_get_used_size(chan->send_queue);
        // }
        // if (read_size != NULL) {
        //     *read_size = tuya_mbuf_queue_get_used_size(chan->recv_queue);
        // }
        if (send_free_size != NULL) {
            *send_free_size = 160 * 1024 /*tuya_mbuf_queue_get_free_size(chan->send_queue)*/;
        }
    } else {
        ret = TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    pthread_mutex_unlock(&rtc->channel_lock);
    tal_mutex_unlock(g_p2p_session_mutex);
    return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////

int rtc_init_mbedtls_md_and_aes(tuya_p2p_rtc_session_t *rtc)
{
    if (rtc->cfg.role == PJ_ROLE_CALLEE /*RTC_ROLE_CALLEE*/) {
        int ret = tuya_p2p_rtc_sdp_get_aes_key(&rtc->remote_sdp, rtc->aes_key, sizeof(rtc->aes_key));
        if (ret < 0) {
            return -1;
        }
    }

    // rtc_init_crypt(rtc);
    for (int i = 0; i < rtc->cfg.channel_number + 1; i++) {
        int ret = rtc_channel_aes_init(&rtc->channels[i]);
        if (ret < 0) {
            return -1;
        }
    }

    tuya_p2p_misc_rand_hex((char *)rtc->iv, sizeof(rtc->iv));

    rtc->md_info = (mbedtls_md_info_t *)mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if (rtc->md_info == NULL) {
        return -1;
    }
    mbedtls_md_setup(&rtc->md_ctx, rtc->md_info, 1);

    return 0;
}

int rtc_channel_aes_init(rtc_channel_t *chan)
{
    tuya_p2p_rtc_session_t *rtc = chan->rtc;
    if (chan->aes_ctx_enc == NULL || chan->aes_ctx_dec == NULL) {
        chan->aes_ctx_enc = (mbedtls_aes_context *)malloc(sizeof(mbedtls_aes_context));
        chan->aes_ctx_dec = (mbedtls_aes_context *)malloc(sizeof(mbedtls_aes_context));
        if (chan->aes_ctx_enc == NULL || chan->aes_ctx_dec == NULL) {
            tuya_p2p_log_error("aes_ctx_enc or aes_ctx_dec is null\n");
            return -1;
        }
        memset(chan->aes_ctx_enc, 0, sizeof(mbedtls_aes_context));
        memset(chan->aes_ctx_dec, 0, sizeof(mbedtls_aes_context));
        mbedtls_aes_init(chan->aes_ctx_enc);
        mbedtls_aes_init(chan->aes_ctx_dec);
        int ret;
        ret = mbedtls_aes_setkey_enc(chan->aes_ctx_enc, rtc->aes_key, sizeof(rtc->aes_key) * 8);
        if (ret != 0) {
            tuya_p2p_log_error("mbedtls_aes_setkey_enc failed\n");
            return -1;
        }
        ret = mbedtls_aes_setkey_dec(chan->aes_ctx_dec, rtc->aes_key, sizeof(rtc->aes_key) * 8);
        if (ret != 0) {
            tuya_p2p_log_error("mbedtls_aes_setkey_dec failed\n");
            return -1;
        }
    }
    return 0;
}

int rtc_crypt_encrypt_aes_128_cbc(struct tuya_p2p_rtc_session *rtc, void *ctx, size_t length, unsigned char *iv,
                                  const unsigned char *input, unsigned char *output)
{
    int ret = -1;
    if (ctx != NULL) {
        ret = mbedtls_aes_crypt_cbc(ctx, MBEDTLS_AES_ENCRYPT, length, iv, input, output);
    } else {
        tuya_p2p_log_error("aes_ctx_enc is null\n");
    }
    return ret;
}

int rtc_crypt_decrypt_aes_128_cbc(struct tuya_p2p_rtc_session *rtc, void *ctx, size_t length, unsigned char *iv,
                                  const unsigned char *input, unsigned char *output)
{
    int ret = -1;
    if (ctx != NULL) {
        ret = mbedtls_aes_crypt_cbc(ctx, MBEDTLS_AES_DECRYPT, length, iv, input, output);
    } else {
        tuya_p2p_log_error("aes_ctx_dec is null\n");
    }
    return ret;
}

int rtc_channel_aes_uninit(struct rtc_channel *chan)
{
    if (chan->aes_ctx_enc != NULL) {
        mbedtls_aes_free(chan->aes_ctx_enc);
        free(chan->aes_ctx_enc);
        chan->aes_ctx_enc = NULL;
    }
    if (chan->aes_ctx_dec != NULL) {
        mbedtls_aes_free(chan->aes_ctx_dec);
        free(chan->aes_ctx_dec);
        chan->aes_ctx_dec = NULL;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

uint32_t tuya_p2p_rtc_get_version()
{
    return (uint32_t)TUYA_P2P_SDK_VERSION_NUMBER;
}

uint32_t tuya_p2p_rtc_get_skill()
{
    return g_uP2PSkill;
}

int32_t tuya_p2p_rtc_deinit()
{
    if (g_pRtcSession == NULL) {
        return TUYA_P2P_ERROR_NOT_INITIALIZED;
    }
    //tal_mutex_lock(g_p2p_session_mutex);
    ctx_session_destroy(g_pRtcSession);
    g_pRtcSession = NULL;
    //tal_mutex_unlock(g_p2p_session_mutex);
    tal_mutex_release(g_p2p_session_mutex);
    g_p2p_session_mutex = NULL;
    return 0;
}

int32_t tuya_p2p_rtc_connect(char *remote_id, char *token, uint32_t token_len, char *trace_id, int lan_mode,
                             int timeout_ms)
{
    int ret = -1;
    return ret;
}

int32_t tuya_p2p_rtc_listen_break()
{
    return 0;
}

int32_t tuya_p2p_rtc_get_session_info(int32_t handle, tuya_p2p_rtc_session_info_t *info)
{
    return 0;
}

// int32_t tuya_p2p_rtc_close(int32_t handle, int32_t reason)
// {
//     return 0;
// }

int32_t tuya_p2p_rtc_send_frame(int32_t handle, tuya_p2p_rtc_frame_t *frame)
{
    return 0;
}

int32_t tuya_p2p_rtc_recv_frame(int32_t handle, tuya_p2p_rtc_frame_t *frame)
{
    return 0;
}

///////////////////////////////////////////////////////////////////////
// logs

static const char *level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

void tuya_p2p_log_log(int level, const char *file, int line, const char *fmt, ...)
{
    // if (TUYA_P2P_LOG_DEBUG < tuya_p2p_log_get_level()) {
    //     return;
    // }

    static char buf[8096] = {0};
    memset(buf, 0, sizeof(buf));
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);

    cJSON *jlog = cJSON_CreateObject();
    cJSON *jt = cJSON_CreateNumber(tuya_p2p_misc_get_timestamp_ms());
    cJSON *jp = cJSON_CreateString(level_names[level]);
    cJSON *jm = cJSON_CreateString(buf);
    // cJSON *jf = cJSON_CreateString(file);
    cJSON *jl = cJSON_CreateNumber(line);

    cJSON_AddItemToObject(jlog, "t", jt);
    // cJSON_AddItemToObject(jlog, "f", jf);
    cJSON_AddItemToObject(jlog, "l", jl);
    cJSON_AddItemToObject(jlog, "p", jp);
    cJSON_AddItemToObject(jlog, "m", jm);

    if (jlog != NULL) {
        char *slog = cJSON_PrintUnformatted(jlog);
        if (slog != NULL) {
            // tuya_p2p_upload_log(level > TUYA_P2P_LOG_DEBUG ? TUYA_P2P_LOG_DEBUG : level, slog, strlen(slog));
            cJSON_free(slog);
        }

        cJSON_Delete(jlog);
    }
    va_end(args);
}

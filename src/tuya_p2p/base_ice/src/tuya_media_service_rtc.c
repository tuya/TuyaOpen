#include "tuya_media_service_rtc.h"
#include "tal_mutex.h"
#include "tuya_mbuf.h"
#include "bc_msg_queue.h"
#include <limits.h>
#include "tal_mutex.h"
#include "tal_semaphore.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tuya_cloud_types.h"
#include "tal_memory.h"

typedef struct {
    void *(*proc)(void *);
    void *arg;
    SEM_HANDLE join_sem;
} __tal_thr_wrap_t;

static void __tal_thr_entry(void *arg)
{
    __tal_thr_wrap_t *w = (__tal_thr_wrap_t *)arg;
    SEM_HANDLE join = NULL;
    if (w != NULL) {
        join = w->join_sem;
        if (w->proc != NULL) {
            (void)w->proc(w->arg);
        }
        tal_free(w);
    }
    if (join != NULL) {
        tal_semaphore_post(join);
    }
}

static int __tal_thread_spawn(THREAD_HANDLE *tid, SEM_HANDLE *join_sem, void *(*proc)(void *), void *arg, uint32_t stack,
                              const char *name)
{
    THREAD_CFG_T cfg;
    __tal_thr_wrap_t *w;
    if (tid == NULL || proc == NULL || join_sem == NULL) {
        return -1;
    }
    w = (__tal_thr_wrap_t *)tal_malloc(sizeof(*w));
    if (w == NULL) {
        return -1;
    }
    if (tal_semaphore_create_init(join_sem, 0, 1) != OPRT_OK) {
        tal_free(w);
        return -1;
    }
    w->proc = proc;
    w->arg = arg;
    w->join_sem = *join_sem;
    memset(&cfg, 0, sizeof(cfg));
    cfg.stackDepth = stack ? stack : (24 * 1024);
    cfg.priority = THREAD_PRIO_2;
    cfg.thrdname = (char *)(name ? name : "rtc");
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    /* Align OS tuya_p2p_lib_pthread_create → tkl_thread_create_in_psram */
    cfg.psram_mode = 1;
#endif
    if (tal_thread_create_and_start(tid, NULL, NULL, __tal_thr_entry, w, &cfg) != OPRT_OK) {
        tal_semaphore_release(*join_sem);
        *join_sem = NULL;
        tal_free(w);
        return -1;
    }
    /* wrap freed? keep until thread ends — leak one small struct per thread lifetime; free at join */
    return 0;
}

static void __tal_thread_join(THREAD_HANDLE tid, SEM_HANDLE *join_sem)
{
    (void)tid;
    if (join_sem && *join_sem) {
        tal_semaphore_wait(*join_sem, SEM_WAIT_FOREVER);
        tal_semaphore_release(*join_sem);
        *join_sem = NULL;
    }
}

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#if defined(__linux__) && !defined(__APPLE__)
#include <sys/prctl.h>
#endif
#include "ikcp.h"
#include "ikcp_pacing.h"
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
#include <pj/errno.h>
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
/* Align OS ctx worker: pop buffer 8KB; ICE offer runs on this thread → large stack */
#define CTX_SIG_WORKER_STACK_SIZE     (48 * 1024)
#define CTX_SIG_WORKER_MSG_BUF_SIZE   8192
#define CTX_SIG_MSG_TYPE_INCOMING     0
#define CTX_SIG_MSG_TYPE_ICE_TIMEOUT  1
/* used_size is payload bytes; warn when backlog is large (not message count) */
#define CTX_SIG_Q_BYTES_WARN          (8 * 1024)

// CMD is transmitted using kcp's channel number field, and kcp uses little endian
#define RTC_CHANNEL_CMD   (0x010000F3)
#define RTC_CMD_SIGNALING (0x0001)

#define P2P_UPLOAD_LOG_MASK_OPEN      0x01
#define P2P_UPLOAD_LOG_MASK_HANDSHAKE 0x02
#define P2P_UPLOAD_LOG_MASK_CLOSE     0x04
#define P2P_UPLOAD_LOG_MASK_ACTIVATE  0x08

#define RTC_TOKEN_REFRESH_INTERVAL_SECONDS 600

#define P2P_DEFAULT_FRAGEMENT_LEN 1300
/* Per-send encrypt scratch (p2p_media_send thread); avoids malloc storm when KCP backs up */
#define DOSEND_ENCRYPT_SCRATCH_SIZE 2048

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
    tuya_mbuf_queue_t *send_queue;
    tuya_mbuf_queue_t *recv_queue;
    int has_receiver;
    ikcpcb *kcp;
    int channel_id;
    uint32_t send_buf_capacity;
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
    MUTEX_HANDLE ref_lock;

    sync_cond_t syncCondExit;

    rtc_sdp_t local_sdp;
    rtc_sdp_t remote_sdp;
    pjmedia_sdp_session *pLocalSdp;
    pjmedia_sdp_session *pRemoteSdp;
    MUTEX_HANDLE channel_lock;
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
    THREAD_HANDLE tid;
    SEM_HANDLE tid_join;
    bool bQuitKCPThread;
    /* Dedup MQTT/LAN duplicate offer: answer already sent for this session */
    int answer_sent;
} tuya_p2p_rtc_session_t;

tuya_p2p_rtc_options_t g_options;
static uint32_t g_uP2PSkill = TUYA_P2P_SDK_SKILL_BASIC /*TUYA_P2P_SDK_SKILL_NUMBER*/;
tuya_p2p_rtc_session_t *g_pRtcSession = NULL;
MUTEX_HANDLE            g_p2p_session_mutex = NULL;
/* Session being destroyed: notify_exit may race after g_pRtcSession cleared */
static tuya_p2p_rtc_session_t *s_exiting_rtc = NULL;
rtc_session_cfg_t cfg;
pj_ice_session_cfg_t iceSessionCfg;
/* OS mid_p2p: msg_queue_incoming + tuya_ctx_worker_thread_func */
static bc_msg_queue_t *s_msg_queue_incoming = NULL;
static THREAD_HANDLE s_sig_worker_tid;
static SEM_HANDLE s_sig_worker_join;
static int s_sig_worker_started = 0;
static volatile int s_sig_worker_quit = 0;

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
static int tuya_p2p_process_signal_msg(char *msg, int msglen);
static void *ctx_signaling_worker_thread(void *arg);
static int ctx_init_msg_queue_and_worker(void);
static void ctx_deinit_msg_queue_and_worker(void);
static void ctx_deinit_msg_queue_and_worker(void);
/**
 * @brief Destroy current RTC session and drop session mutex (sig-worker only)
 * @return none
 */
static void __rtc_destroy_current_session(const char *reason)
{
    PR_NOTICE("p2p session destroy reason=%s", reason ? reason : "?");
    ctx_session_destroy(g_pRtcSession);
    if (g_p2p_session_mutex != NULL) {
        tal_mutex_release(g_p2p_session_mutex);
        g_p2p_session_mutex = NULL;
    }
}
/**
 * @brief Whether current session ICE is ready to keep on duplicate offer
 * @param[in] rtc session
 * @return 1 if RUNNING, else 0
 */
static int __rtc_ice_media_ready(tuya_p2p_rtc_session_t *rtc)
{
    if (rtc == NULL || rtc->pIce == NULL) {
        return 0;
    }
    return pj_ice_session_is_nego_success(rtc->pIce) ? 1 : 0;
}
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
/**
 * @brief Dispatch outbound signaling via LAN or MQTT based on session lan_mode
 * @param[in] rtc session handle
 * @param[in] cfg session cfg (remote_id / lan_mode); may equal &rtc->cfg
 * @param[in] signaling JSON body (without requiring path field)
 * @return none
 */
static void ctx_session_dispatch_signaling(tuya_p2p_rtc_session_t *rtc, rtc_session_cfg_t *cfg, char *signaling)
{
    char *to_send = signaling;
    char *with_path = NULL;

    if ((rtc == NULL) || (cfg == NULL) || (signaling == NULL)) {
        return;
    }
    if ((cfg->lan_mode != 0) && (rtc->cb.on_lan_signaling != NULL)) {
        with_path = ctx_signaling_add_path(signaling, "lan");
        if (with_path != NULL) {
            to_send = with_path;
        }
        rtc->cb.on_lan_signaling(cfg->remote_id, to_send, (uint32_t)strlen(to_send));
        PR_DEBUG("lan signaling send ok");
    } else if (rtc->cb.on_signaling != NULL) {
        rtc->cb.on_signaling(cfg->remote_id, signaling, (uint32_t)strlen(signaling));
        PR_DEBUG("mqtt signaling send ok");
    }
    if (with_path != NULL) {
        cJSON_free(with_path);
    }
}
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
    /* Align OS: incoming signaling queue + worker (LAN/MQTT only enqueue) */
    if (ctx_init_msg_queue_and_worker() != 0) {
        PR_ERR("tuya_ctx_init_msg_queue / worker failed");
        return TUYA_P2P_ERROR_FAIL_TO_CREATE_THREAD;
    }
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
        PR_ERR("invalid webrtc signaling: not a json");
        return -1;
    }

    // parse header
    cJSON *el_header = cJSON_GetObjectItemCaseSensitive(root, "header");
    if (!cJSON_IsObject(el_header)) {
        PR_ERR("invalid signaling: no header field");
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
        PR_ERR("invalid signaling: invalid header");
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
        PR_ERR("invalid signaling: no msg field");
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

    PR_NOTICE("process signaling %s", type);
    // create new session if necessary
    // static pj_session_cfg_t cfg;
    // tuya_p2p_rtc_session_t *rtc = ctx_session_get(ctx, remote_id, session_id);
    /*
     * Reconnect / duplicate offer:
     * - Different session_id while old still alive: destroy old first, then create
     * - Same session_id but ICE not RUNNING: App retry while stuck — destroy and recreate
     * - Same session_id and ICE RUNNING: keep session (answer dedup below)
     */
    if (strcmp(type, "offer") == 0 && g_pRtcSession != NULL) {
        if (strcmp(g_pRtcSession->cfg.session_id, session_id) != 0) {
            PR_NOTICE("p2p new offer replaces session");
            __rtc_destroy_current_session("offer_new_session_id");
        } else if (!__rtc_ice_media_ready(g_pRtcSession)) {
            __rtc_destroy_current_session("offer_same_session_ice_stuck");
        } else {
        }
    }
    if (g_pRtcSession == NULL && strcmp(type, "offer") == 0) {
        memset(&cfg, 0, sizeof(cfg));

        if (cJSON_IsString(el_mode) && strcmp(el_mode->valuestring, "webrtc") == 0) {
            cfg.is_webrtc = 1;
        } else {
            // cfg.channel_number = ctx->opt.max_channel_number; // todo: set channel number
        }

        if (!cJSON_IsArray(el_token) && !cJSON_IsObject(el_udp_token) && !cJSON_IsObject(el_tcp_token)) {
            PR_ERR("invalid signaling: no token field");
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
            PR_DEBUG("security_level in offer: %d", el_security_level->valueint);
        } else {
            PR_DEBUG("no security_level in offer, use default L3");
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
        cfg.lan_mode = (strcmp(str_path, "lan") == 0) ? 1 : 0;
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

        {
            if (__tal_thread_spawn(&g_pRtcSession->tid, &g_pRtcSession->tid_join, rtc_worker_thread, g_pRtcSession,
                                   24 * 1024, "rtc_wrk") != 0) {
                PR_ERR("rtc_worker create failed");
            }
        }
        tuya_p2p_log_info("ctx_session_create\n");
    }

    if (g_pRtcSession == NULL) {
        tuya_p2p_log_info("can not find rtc session\n");
        if (root != NULL) {
            cJSON_Delete(root);
        }
        return -1;
    }
    /* Keep LAN reply path when App retries/signals over LAN */
    if (strcmp(str_path, "lan") == 0) {
        g_pRtcSession->cfg.lan_mode = 1;
    }

    if (strcmp(type, "candidate") == 0) {
        tuya_p2p_rtc_session_t *rtc_cand = NULL;
        if (!cJSON_IsString(el_candidate)) {
            PR_ERR("invalid signaling: type candidate");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        /*
         * Never hold g_p2p_session_mutex across ICE update_check_list / start_ice.
         * ICE worker callbacks (on_new_candidate) also send signaling and used to
         * take the same mutex → deadlock (hang after "local_cand send", then reboot).
         */
        if (g_p2p_session_mutex != NULL) {
            tal_mutex_lock(g_p2p_session_mutex);
            rtc_cand = g_pRtcSession;
            tal_mutex_unlock(g_p2p_session_mutex);
        } else {
            rtc_cand = g_pRtcSession;
        }
        if (rtc_cand != NULL) {
            if (strcmp(rtc_cand->cfg.session_id, session_id) != 0) {
            } else {
                ctx_session_add_remote_candidate(rtc_cand, &rtc_cand->remote_sdp, el_candidate->valuestring);
            }
        }
    } else if (strcmp(type, "offer") == 0) {
        tuya_p2p_rtc_session_t *rtc_offer = NULL;
        int skip_dup_offer = 0;
        if (!cJSON_IsString(el_sdp)) {
            PR_ERR("invalid signaling: type sdp");
            if (root != NULL) {
                cJSON_Delete(root);
            }
            return -1;
        }
        char *buf = el_sdp->valuestring;
        if (g_p2p_session_mutex != NULL) {
            tal_mutex_lock(g_p2p_session_mutex);
            rtc_offer = g_pRtcSession;
            /* Only skip when ICE already RUNNING; stuck ICE was destroyed above */
            if (rtc_offer != NULL && rtc_offer->answer_sent && __rtc_ice_media_ready(rtc_offer)) {
                skip_dup_offer = 1;
            } else if (rtc_offer != NULL) {
                tuya_p2p_rtc_sdp_decode(&rtc_offer->remote_sdp, buf);
                tuya_p2p_rtc_sdp_negotiate(&rtc_offer->local_sdp, &rtc_offer->remote_sdp, type);
            }
            tal_mutex_unlock(g_p2p_session_mutex);
        } else {
            rtc_offer = g_pRtcSession;
            if (rtc_offer != NULL && rtc_offer->answer_sent && __rtc_ice_media_ready(rtc_offer)) {
                skip_dup_offer = 1;
            } else if (rtc_offer != NULL) {
                tuya_p2p_rtc_sdp_decode(&rtc_offer->remote_sdp, buf);
                tuya_p2p_rtc_sdp_negotiate(&rtc_offer->local_sdp, &rtc_offer->remote_sdp, type);
            }
        }
        if (skip_dup_offer) {
        } else if (rtc_offer != NULL) {
            /* Send answer without session mutex (may race with ICE trickle send) */
            ctx_session_send_sdp(rtc_offer, &cfg);
            rtc_offer->answer_sent = 1;
        }
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
        PR_NOTICE("p2p disconnect reason=%d", close_reason);
        __rtc_destroy_current_session("mqtt_disconnect");
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

/**
 * @brief Create incoming signaling queue and worker (OS tuya_ctx_init_msg_queue + worker)
 * @return 0 on success, -1 on failure
 */
static int ctx_init_msg_queue_and_worker(void)
{
    int ret;

    if (s_msg_queue_incoming != NULL) {
        return 0;
    }
    s_msg_queue_incoming = bc_msg_queue_create();
    if (s_msg_queue_incoming == NULL) {
        return -1;
    }
    s_sig_worker_quit = 0;
    ret = __tal_thread_spawn(&s_sig_worker_tid, &s_sig_worker_join, ctx_signaling_worker_thread, NULL,
                             CTX_SIG_WORKER_STACK_SIZE, "ctx_sig");
    if (ret != 0) {
        bc_msg_queue_destroy(s_msg_queue_incoming);
        s_msg_queue_incoming = NULL;
        return -1;
    }
    s_sig_worker_started = 1;
    PR_NOTICE("p2p rtc worker start");
    return 0;
}

/**
 * @brief Stop signaling worker and destroy incoming queue
 * @return none
 */
static void ctx_deinit_msg_queue_and_worker(void)
{
    if (s_msg_queue_incoming != NULL) {
        s_sig_worker_quit = 1;
        bc_msg_queue_close(s_msg_queue_incoming);
    }
    if (s_sig_worker_started != 0) {
        __tal_thread_join(s_sig_worker_tid, &s_sig_worker_join);
        s_sig_worker_started = 0;
    }
    if (s_msg_queue_incoming != NULL) {
        bc_msg_queue_destroy(s_msg_queue_incoming);
        s_msg_queue_incoming = NULL;
    }
}

/**
 * @brief Worker: dequeue signaling and run ICE/session logic (OS tuya_ctx_worker_thread_func)
 * @param[in] arg unused
 * @return NULL
 */
static void *ctx_signaling_worker_thread(void *arg)
{
    char *buf;
    int type;
    int len;
    int n;

    (void)arg;
    buf = (char *)malloc(CTX_SIG_WORKER_MSG_BUF_SIZE);
    if (buf == NULL) {
        PR_ERR("p2p rtc worker malloc err");
        return NULL;
    }
    while (s_sig_worker_quit == 0) {
        type = 0;
        len = CTX_SIG_WORKER_MSG_BUF_SIZE - 1;
        n = bc_msg_queue_pop_front(s_msg_queue_incoming, &type, buf, &len);
        if (n < 0) {
            break;
        }
        if (len >= (CTX_SIG_WORKER_MSG_BUF_SIZE - 1)) {
            PR_ERR("mqtt msg too large: %d", len);
            continue;
        }
        buf[len] = '\0';
        if (type == CTX_SIG_MSG_TYPE_INCOMING) {
            int qbytes = bc_msg_queue_get_length(s_msg_queue_incoming);
            uint64_t t0 = tuya_p2p_misc_get_timestamp_ms();
            (void)tuya_p2p_process_signal_msg(buf, len);
        } else if (type == CTX_SIG_MSG_TYPE_ICE_TIMEOUT) {
            if (g_pRtcSession != NULL && !__rtc_ice_media_ready(g_pRtcSession)) {
                __rtc_destroy_current_session("ice_nego_timeout");
            } else {
            }
        }
    }
    free(buf);
    return NULL;
}

int32_t tuya_p2p_rtc_set_signaling(char *remote_id, char *msg, uint32_t msglen)
{
    int ret;
    int qlen = -1;

    (void)remote_id;
    if ((msg == NULL) || (msglen == 0)) {
        return -1;
    }
    if (s_msg_queue_incoming == NULL) {
        PR_ERR("tuya_p2p_rtc_set_signaling failed: sdk not init");
        return -1;
    }
    qlen = bc_msg_queue_get_length(s_msg_queue_incoming);
    /* Align OS: only enqueue; worker runs tuya_p2p_process_signal_msg */
    ret = bc_msg_queue_push_back(s_msg_queue_incoming, CTX_SIG_MSG_TYPE_INCOMING, msg, (int)msglen);
    if (ret != 0) {
        PR_ERR("tuya_p2p_rtc_set_signaling push failed ret=%d", ret);
        return -1;
    }
    if (qlen >= CTX_SIG_Q_BYTES_WARN) {
        PR_WARN("p2p signaling backlog qbytes=%d", qlen);
    }
    return 0;
}

int ctx_session_add_remote_candidate(tuya_p2p_rtc_session_t *rtc, rtc_sdp_t *remote_sdp, char *candidate)
{
    pj_ice_session_t *pIceSession = rtc->pIce;
    int iCandidateLen = strlen(candidate);
    char buf[PJ_INET6_ADDRSTRLEN + 10] = {0};

    if (candidate == NULL || iCandidateLen == 0) {
        return -1;
    }
    /* After ICE done, ignore late MQTT/LAN trickle duplicates */
    if (pIceSession != NULL && pj_ice_session_is_nego_done(pIceSession)) {
        return 0;
    }
    pj_str_t pjstrCand = pj_str(candidate);
    if (iCandidateLen >= 2 && *(candidate + iCandidateLen - 2) == '\r' &&
        *(candidate + iCandidateLen - 1) == '\n') {
        pjstrCand.slen -= 2; // Remove trailing \r\n character length
    }
    pj_ice_sess_cand cand;
    if (parse_cand(NULL, &pjstrCand, &cand) !=
        0 /*|| cand.type == PJ_ICE_CAND_TYPE_HOST || cand.type == PJ_ICE_CAND_TYPE_RELAYED*/) {
        return -1;
    }
    /* IPv6 not used on T5 LAN path — skip before SDP queue malloc */
    if (cand.addr.addr.sa_family == pj_AF_INET6()) {
        return 0;
    }
    /* Only queue parse-OK IPv4 candidates (string-dedup inside) */
    tuya_p2p_rtc_sdp_add_candidate(remote_sdp, candidate);
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
    /*
     * Invoke MQTT/LAN send without g_p2p_session_mutex. Holding that lock here
     * deadlocks with signaling thread blocked inside ICE add_remote.
     */
    ctx_session_dispatch_signaling(rtc, cfg, signaling);

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
    /* Do not hold g_p2p_session_mutex across MQTT/LAN send (ICE worker deadlock). */
    ctx_session_dispatch_signaling(rtc, cfg, signaling);

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
    ctx_session_dispatch_signaling(rtc, &rtc->cfg, signaling);

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
    if (rtc == NULL) {
        return 0;
    }
    ctx_session_dispatch_signaling(rtc, &rtc->cfg, signaling);
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
    char errmsg[PJ_ERR_MSG_SIZE];

    if (!pIceSession)
        return;

    pj_strerror(status, errmsg, sizeof(errmsg));

    switch (op) {
    case PJ_ICE_STRANS_OP_INIT: {
        if (status != PJ_SUCCESS) {
            PR_ERR("ICE gather/init failed status=%d(%s)", (int)status, errmsg);
        } else {
            /* Local STUN/TURN gather finished; start ICE if remote offer already arrived */
            (void)pj_ice_session_on_local_gather_done(pIceSession);
        }
        break;
    }
    case PJ_ICE_STRANS_OP_NEGOTIATION: {
        if (status == PJ_SUCCESS) {
            char szLCandAddr[PJ_INET6_ADDRSTRLEN + 10] = {0};
            char szRCandAddr[PJ_INET6_ADDRSTRLEN + 10] = {0};
            unsigned comp_id = 1; // Component starting ID number is 1
            const pj_ice_sess_check *pIceSessCheck = pj_ice_strans_get_valid_pair(ice_st, comp_id);
            if (pIceSessCheck == NULL || pIceSessCheck->lcand == NULL || pIceSessCheck->rcand == NULL) {
                PR_ERR("ICE negotiation OK but valid pair missing");
            } else {
                pj_sockaddr_print(&pIceSessCheck->lcand->addr, szLCandAddr, sizeof(szLCandAddr), 3);
                pj_sockaddr_print(&pIceSessCheck->rcand->addr, szRCandAddr, sizeof(szRCandAddr), 3);
            }

            {
                int crypt_ret = rtc_init_mbedtls_md_and_aes(pRtcSession);
                if (crypt_ret != 0) {
                    PR_ERR("p2p crypt init failed");
                }
            }
            sync_cond_notify(&g_syncCond);
            PR_NOTICE("ICE negotiation success lcand=%s rcand=%s", szLCandAddr, szRCandAddr);
        } else {
            PR_ERR("ICE negotiation failed status=%d(%s)", (int)status, errmsg);
        }
        break;
    }
    case PJ_ICE_STRANS_OP_KEEP_ALIVE: {
        if (status != PJ_SUCCESS) {
            PR_WARN("ICE keep-alive failed status=%d(%s)", (int)status, errmsg);
        }
        break;
    }
    default: {
        PR_WARN("ICE complete unknown op=%d status=%d", (int)op, (int)status);
        break;
    }
    }

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

    /*
     * Host candidates are added during create but not trickled via this cb.
     * When gathering finishes, dump all session local candidates (correct prio)
     * so LAN host and relay are advertised to the peer.
     */
    if (last) {
        pj_ice_sess_cand *cands = NULL;
        char *szCand = NULL;
        unsigned count = PJ_ICE_MAX_CAND;
        unsigned i;
        unsigned host_n = 0, srflx_n = 0, relay_n = 0;
        pj_status_t st;
        char buf[PJ_INET6_ADDRSTRLEN + 10];

        /* Heap buffers: stack overflow was observed on 8KB rtc_worker */
        cands = (pj_ice_sess_cand *)malloc(sizeof(pj_ice_sess_cand) * PJ_ICE_MAX_CAND);
        szCand = (char *)malloc(1024);
        if (cands == NULL || szCand == NULL) {
            free(cands);
            free(szCand);
            return;
        }
        st = pj_ice_strans_enum_cands(ice_st, 1, &count, cands);
        if (st != PJ_SUCCESS) {
            free(cands);
            free(szCand);
            return;
        }
        for (i = 0; i < count; ++i) {
            if (cands[i].type == PJ_ICE_CAND_TYPE_HOST) {
                host_n++;
            } else if (cands[i].type == PJ_ICE_CAND_TYPE_SRFLX) {
                srflx_n++;
            } else if (cands[i].type == PJ_ICE_CAND_TYPE_RELAYED) {
                relay_n++;
            }

            /* Skip IPv6 on IPv4-only builds / when family unsupported */
            if (cands[i].addr.addr.sa_family != pj_AF_INET()) {
                continue;
            }
            memset(szCand, 0, 1024);
            if (print_cand(szCand, 1024, &cands[i]) < 0) {
                continue;
            }
            ctx_session_send_candidate(pRtcSession, &cfg, szCand);
        }
        if (relay_n == 0) {
        }
        free(cands);
        free(szCand);

        /* Trickle send done; retry start_ice in case remote ufrag arrived earlier */
        (void)pj_ice_session_try_start_ice(pIceSession, "local_cand_last");
    }

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
            PR_ERR("p2p invalid HMAC ch=%u", channel_id);
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
        if (!pj_ice_session_sendto(rtc->pIce, (void *)buf, len + md_size)) {
#if IKCP_PACING_RATE_LIMIT
            /* UDP ENOBUFS / send fail: back off pacing so flush stops blasting */
            pacing_on_send_fail(kcp, 50);
#endif
        }
    }

    chan->socket_send_bytes += (len + md_size);
    return len;
}

void rtc_ref_cnt_add(tuya_p2p_rtc_session_t *rtc) {
    tal_mutex_lock(rtc->ref_lock);
    rtc->ref_cnt++;
    tal_mutex_unlock(rtc->ref_lock);
}

void rtc_ref_cnt_del(tuya_p2p_rtc_session_t *rtc) {
    tal_mutex_lock(rtc->ref_lock);
    rtc->ref_cnt--;
    tal_mutex_unlock(rtc->ref_lock);
}

int rtc_ref_cnt_get(tuya_p2p_rtc_session_t *rtc) {
    int ref_cnt;
    tal_mutex_lock(rtc->ref_lock);
    ref_cnt = rtc->ref_cnt;
    tal_mutex_unlock(rtc->ref_lock);
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
    tal_mutex_create_init(&rtc->ref_lock);
    tal_mutex_create_init(&rtc->channel_lock);
    rtc->cfg.channel_number = 3;
    rtc->active_handle = 0;
    rtc->local_cmd_seq = 0;
    rtc->tid = NULL;
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
    int wait_i;
    uint64_t t0;

    if (rtc == NULL) {
        return;
    }

    /* 1) Detach from global so new recv/send fail; keep rtc alive for in-flight users */
    if (g_p2p_session_mutex != NULL) {
        tal_mutex_lock(g_p2p_session_mutex);
    }
    if (g_pRtcSession == rtc) {
        g_pRtcSession = NULL;
    }
    s_exiting_rtc = rtc;
    rtc->bQuitKCPThread = true;
    if (g_p2p_session_mutex != NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
    }

    /* 2) Stop ICE/KCP worker before tearing channels */
    if (rtc->tid != NULL) {
        t0 = tuya_p2p_misc_get_timestamp_ms();
        __tal_thread_join(rtc->tid, &rtc->tid_join);
        rtc->tid = NULL;
    }

    /*
     * 3) Wait upper layer (p2p_cmd_recv) to leave dorecv and call notify_exit.
     *    Must happen BEFORE channels_destroy or cmd_recv UAF on kcp.
     */
    t0 = tuya_p2p_misc_get_timestamp_ms();
    for (wait_i = 0; wait_i < 300; wait_i++) {
        int met = 0;
        tal_mutex_lock(rtc->syncCondExit.mutex);
        met = rtc->syncCondExit.condition_met;
        tal_mutex_unlock(rtc->syncCondExit.mutex);
        if (met != 0) {
            break;
        }
        /* Proceed when only create's base ref remains (recv/send drained) */
        if (rtc_ref_cnt_get(rtc) <= 1) {
            sync_cond_notify(&rtc->syncCondExit);
            break;
        }
        if ((wait_i % 50) == 49) {
        }
        tal_system_sleep((10 * 1000 + 999) / 1000);
    }
    if (rtc->syncCondExit.condition_met == 0) {
        sync_cond_notify(&rtc->syncCondExit);
    }
    sync_cond_wait(&rtc->syncCondExit);
    sync_cond_clean(&rtc->syncCondExit);

    /* Drain any late recv/send refs before free */
    for (wait_i = 0; wait_i < 100 && rtc_ref_cnt_get(rtc) > 1; wait_i++) {
        if ((wait_i % 20) == 0) {
        }
        tal_system_sleep((10 * 1000 + 999) / 1000);
    }

    /* 4) Tear channels/ICE under channel_lock */
    tal_mutex_lock(rtc->channel_lock);
    tuya_p2p_rtc_channels_destroy(rtc);
    tal_mutex_unlock(rtc->channel_lock);

    if (rtc->pIce != NULL) {
        pj_ice_session_destroy(rtc->pIce);
        rtc->pIce = NULL;
    } else {
    }

    tuya_p2p_rtc_sdp_deinit(&rtc->local_sdp);
    tuya_p2p_rtc_sdp_deinit(&rtc->remote_sdp);
    mbedtls_md_free(&rtc->md_ctx);
    tal_mutex_release(rtc->ref_lock);
    tal_mutex_release(rtc->channel_lock);
    if (s_exiting_rtc == rtc) {
        s_exiting_rtc = NULL;
    }
    free(rtc);
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
    tuya_p2p_rtc_session_t *rtc;

    ctx_session_channel_set_data_time(chan);
    if (chan == NULL || chan->kcp == NULL || chan->rtc == NULL) {
        return 0;
    }
    rtc = chan->rtc;
    tal_mutex_lock(rtc->channel_lock);
    (void)ikcp_input(chan->kcp, (const char *)data, len);
    tal_mutex_unlock(rtc->channel_lock);
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

    if (chan->aes_ctx_dec == NULL) {
        return -1;
    }
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
            unsigned char padding_size = (unsigned char)decrypted[msg_size - 1];
            int pad_ok = 1;
            int i;
            if (padding_size == 0 || padding_size > 16 || padding_size > msg_size) {
                pad_ok = 0;
            } else {
                for (i = 0; i < (int)padding_size; i++) {
                    if ((unsigned char)decrypted[msg_size - 1 - i] != padding_size) {
                        pad_ok = 0;
                        break;
                    }
                }
            }
            if (pad_ok) {
                ret = msg_size - padding_size;
                if (chan->channel_id == 0) {
                    unsigned char *b = (unsigned char *)decrypted;
                }
            } else {
                ret = -1;
            }
        } else {
            ret = -1;
        }
    } else {
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
        chan->send_buf_capacity = send_buf_size;
        chan->send_queue = tuya_mbuf_queue_create((int)send_buf_size, NULL);
        chan->recv_queue = tuya_mbuf_queue_create((int)recv_buf_size, NULL);
        chan->kcp = ikcp_create(channel_id, chan);
        if (chan->kcp == NULL || chan->send_queue == NULL || chan->recv_queue == NULL) {
            goto finish;
        }

        ikcp_setoutput(chan->kcp, on_kcp_output);
        ikcp_wndsize(chan->kcp, send_buf_size / 1600  /*TUYA_MBUF_HUGE_SIZE*/,
                     recv_buf_size / 1600 /*TUYA_MBUF_HUGE_SIZE*/);
        /* Align TuyaOS mid_p2p: ikcp_nodelay(kcp, 0, 5, 20, nc);
         * nc = !preconnect_enable (OS: clz of preconnect field). */
        ikcp_nodelay(chan->kcp, 0, 5, 20, g_options.preconnect_enable ? 0 : 1);
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
            if (chan->send_queue != NULL) {
                tuya_mbuf_queue_destroy(chan->send_queue);
                chan->send_queue = NULL;
            }
            if (chan->recv_queue != NULL) {
                tuya_mbuf_queue_destroy(chan->recv_queue);
                chan->recv_queue = NULL;
            }
            rtc_channel_aes_uninit(chan);
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
    uint64_t start_ms = tuya_p2p_misc_get_timestamp_ms();
    uint64_t last_dump_ms = 0;
    int timeout_logged = 0;
    int nego_done_logged = 0;

    while (!rtc->bQuitKCPThread) {
        if (rtc->pIce != NULL) {
            pj_ice_session_handle_events(rtc->pIce, 5, NULL); // Drive ICE state update and execute KCP receive operation
        }
        for (int i = 0; i < 3; ++i) //(rtc->cfg.channel_number + 1)
        {
            rtc_channel_t *channel = &rtc->channels[i];
            /* Serialize with dosend/ikcp_send on media/cmd threads */
            tal_mutex_lock(rtc->channel_lock);
            if (channel->kcp != NULL) {
                ikcp_update(channel->kcp, tuya_p2p_misc_get_timestamp_ms());
            }
            tal_mutex_unlock(rtc->channel_lock);
        }

        {
            uint64_t now = tuya_p2p_misc_get_timestamp_ms();
            if ((now - last_dump_ms) >= 2000ULL) {
                uint64_t elapsed = now - start_ms;
                last_dump_ms = now;
                if (rtc->pIce != NULL && !pj_ice_session_is_nego_done(rtc->pIce)) {
                    pj_ice_session_dbg_dump(rtc->pIce, "worker_pending");
                    if (!timeout_logged && rtc->cfg.connect_limit_time_ms > 0 &&
                        elapsed >= (uint64_t)rtc->cfg.connect_limit_time_ms) {
                        const char *tm = "ice_timeout";
                        pj_ice_session_dbg_dump(rtc->pIce, "worker_timeout");
                        timeout_logged = 1;
                        /* Do NOT destroy here (would join self). Sig worker tears session down. */
                        if (s_msg_queue_incoming != NULL) {
                            (void)bc_msg_queue_push_back(s_msg_queue_incoming, CTX_SIG_MSG_TYPE_ICE_TIMEOUT, tm,
                                                         (int)strlen(tm));
                        }
                        rtc->bQuitKCPThread = true;
                        break;
                    }
                } else if (rtc->pIce != NULL && !nego_done_logged) {
                    pj_ice_session_dbg_dump(rtc->pIce, "worker_done");
                    nego_done_logged = 1;
                }
            }
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
    /* TuyaOS mid_p2p dosend uses 1300 */
    int fragement_len = (int)g_options.fragement_len;
    if (fragement_len <= 0 || fragement_len > 1300) {
        fragement_len = 1300;
    }

    while (remain > 0) {
        tal_mutex_lock(rtc->channel_lock);
        if (rtc->channels == NULL) {
            tal_mutex_unlock(rtc->channel_lock);
            rc = -1;
            break;
        }
        rtc_channel_t *chan = &rtc->channels[channel_id];
        if (chan->kcp == NULL || chan->send_queue == NULL) {
            tal_mutex_unlock(rtc->channel_lock);
            rc = -1;
            break;
        }
        ctx_session_channel_set_write_time(chan);

        /* OS: wait until mbuf_queue_get_free_size > 0 (usleep 20ms) */
        if (tuya_mbuf_queue_get_status(chan->send_queue) != 0) {
            tal_mutex_unlock(rtc->channel_lock);
            if (already > 0) {
                return already;
            }
            return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
        }
        while (tuya_mbuf_queue_get_free_size(chan->send_queue) <= 0) {
            if (timeout_ms == 0) {
                tal_mutex_unlock(rtc->channel_lock);
                if (already > 0) {
                    return already;
                }
                return TUYA_P2P_ERROR_TIME_OUT;
            }
            if (timeout_ms > 0) {
                if (begin_time == 0) {
                    begin_time = tuya_p2p_misc_get_timestamp_ms();
                }
                if (tuya_p2p_misc_check_timeout(begin_time, timeout_ms)) {
                    tal_mutex_unlock(rtc->channel_lock);
                    if (already > 0) {
                        return already;
                    }
                    return TUYA_P2P_ERROR_TIME_OUT;
                }
            }
            tal_mutex_unlock(rtc->channel_lock);
            tal_system_sleep((20 * 1000 + 999) / 1000);
            tal_mutex_lock(rtc->channel_lock);
            if (rtc->channels == NULL) {
                tal_mutex_unlock(rtc->channel_lock);
                rc = -1;
                goto dosend_out;
            }
            chan = &rtc->channels[channel_id];
            if (chan->kcp == NULL || chan->send_queue == NULL) {
                tal_mutex_unlock(rtc->channel_lock);
                rc = -1;
                goto dosend_out;
            }
        }

        {
            int current = (remain > fragement_len) ? fragement_len : remain;
            int iv_size = (int)sizeof(rtc->iv);
            int keylen = 16;
            int sign_size = 0;
            unsigned char padding_size = (unsigned char)(keylen - (current % keylen));
            int buflen = current + (int)padding_size;
            int wire_len;
            int headroom = 168; /* OS alloc size = payload + 168 */
            tuya_mbuf_t *mbuf;
            char tmp_iv[16];

            if (rtc->cfg.security_level == TUYA_P2P_SECURITY_LEVEL_4) {
                sign_size = 16;
            }
            wire_len = buflen + iv_size + sign_size;
            if (wire_len > 1500) {
                tal_mutex_unlock(rtc->channel_lock);
                return TUYA_P2P_ERROR_OUT_OF_MEMORY;
            }

            mbuf = tuya_mbuf_alloc(chan->send_queue, wire_len + headroom);
            if (mbuf == NULL) {
                tal_mutex_unlock(rtc->channel_lock);
                tal_system_sleep((20 * 1000 + 999) / 1000);
                continue;
            }

            if (chan->aes_ctx_enc == NULL) {
                tuya_mbuf_free(mbuf);
                tal_mutex_unlock(rtc->channel_lock);
                return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
            }

            memcpy(mbuf->data + iv_size, buf + already, (size_t)current);
            memset(mbuf->data + iv_size + current, padding_size, padding_size);
            tuya_p2p_misc_rand_hex(tmp_iv, sizeof(rtc->iv));
            memcpy(mbuf->data, tmp_iv, (size_t)iv_size);

            {
                int ret = rtc_crypt_encrypt_aes_128_cbc(rtc, chan->aes_ctx_enc, buflen, (unsigned char *)tmp_iv,
                                                        (const unsigned char *)(mbuf->data + iv_size),
                                                        (unsigned char *)(mbuf->data + iv_size));
                if (ret != 0) {
                    tuya_mbuf_free(mbuf);
                    tal_mutex_unlock(rtc->channel_lock);
                    tuya_p2p_log_error("aes encrypt failed, ret = %d\n", ret);
                    rc = -1;
                    break;
                }
            }

            mbuf->len = wire_len;
            if (ikcp_send_mbuf(chan->kcp, mbuf, wire_len) < 0) {
                tuya_mbuf_free(mbuf);
                tal_mutex_unlock(rtc->channel_lock);
                if (already > 0) {
                    return already;
                }
                return TUYA_P2P_ERROR_OUT_OF_MEMORY;
            }
            remain -= current;
            already += current;
            chan->write_bytes += current;
        }
        tal_mutex_unlock(rtc->channel_lock);
    }

dosend_out:
    if (already > 0) {
        return already;
    } else if (rc < 0) {
        return TUYA_P2P_ERROR_SESSION_CLOSED_TIMEOUT;
    } else {
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
        tal_mutex_lock(rtc->channel_lock);
        if (rtc->channels == NULL) {
            tal_mutex_unlock(rtc->channel_lock);
            ret = -1;
            break;
        }
        rtc_channel_t *chan = &rtc->channels[channel_id];
        // ret = ikcp_recv_mbufwithdata(chan->kcp, buf, buflen);
        if (0 != ret) {
            chan->read_bytes += ret;
            tal_mutex_unlock(rtc->channel_lock);
            break;
        }
        tal_mutex_unlock(rtc->channel_lock);
        if (timeout_ms >= 0) {
            if (begin_time == 0) {
                begin_time = tuya_p2p_misc_get_timestamp_ms();
            }
            if (tuya_p2p_misc_check_timeout(begin_time, timeout_ms)) {
                break;
            }
        }
        tal_system_sleep((10 * 1000 + 999) / 1000);
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
        tal_mutex_lock(rtc->channel_lock);
        if ((rtc->channels == NULL) || (rtc->bQuitKCPThread)) {
            tal_mutex_unlock(rtc->channel_lock);
            ret = -4;
            break;
        }
        rtc_channel_t *chan = &rtc->channels[channel_id];
        if (chan->kcp == NULL) {
            tal_mutex_unlock(rtc->channel_lock);
            ret = -4;
            break;
        }
        ret = ikcp_recv2(chan->kcp, buf, buflen);
        if (ret > 0) {
            chan->read_bytes += ret;
            *len = ret;
            tal_mutex_unlock(rtc->channel_lock);
            break;
        }
        tal_mutex_unlock(rtc->channel_lock);
        if (timeout_ms >= 0) {
            if (begin_time == 0) {
                begin_time = tuya_p2p_misc_get_timestamp_ms();
            }
            if (tuya_p2p_misc_check_timeout(begin_time, timeout_ms)) {
                break;
            }
        }
        tal_system_sleep((10 * 1000 + 999) / 1000);
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
    int32_t ret;
    tuya_p2p_rtc_session_t *rtc;

    *len = 0;

    tal_mutex_lock(g_p2p_session_mutex);
    rtc = g_pRtcSession;
    if (rtc == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        tuya_p2p_log_error("rtc session %08x recv data: invalid session\n", handle);
        return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    if (channel_id < 0 || channel_id >= rtc->cfg.channel_number) {
        tal_mutex_unlock(g_p2p_session_mutex);
        tuya_p2p_log_error("rtc session %08x recv data: invalid channel number: %d/%d\n", handle, channel_id,
                           rtc->cfg.channel_number);
        return TUYA_P2P_ERROR_INVALID_PARAMETER;
    }
    rtc_ref_cnt_add(rtc);
    tal_mutex_unlock(g_p2p_session_mutex);

    *len = buflen;
    ret = tuya_p2p_rtc_dorecv_data2(rtc, channel_id, buf, len, timeout_ms);
    rtc_ref_cnt_del(rtc);
    return ret;
}

void tuya_p2p_rtc_notify_exit()
{
    tuya_p2p_rtc_session_t *rtc;

    rtc = g_pRtcSession;
    if (rtc == NULL) {
        rtc = s_exiting_rtc;
    }
    if (rtc == NULL) {
        return;
    }
    sync_cond_notify(&rtc->syncCondExit);
}

int32_t tuya_p2p_rtc_check(int32_t handle)
{
    return 0;
}

int32_t tuya_p2p_rtc_check_buffer(int32_t handle, uint32_t channel_id, uint32_t *write_size, uint32_t *read_size,
                                  uint32_t *send_free_size)
{
    int ret = 0;
    (void)handle;
    tal_mutex_lock(g_p2p_session_mutex);
    if (g_pRtcSession == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    tuya_p2p_rtc_session_t *rtc = g_pRtcSession;
    tal_mutex_lock(rtc->channel_lock);
    if (rtc->channels != NULL) {
        rtc_channel_t *chan = &rtc->channels[channel_id];
        /* Align TuyaOS mid_p2p: sizes from mbuf_queue */
        if (write_size != NULL) {
            *write_size = (chan->send_queue != NULL) ? (uint32_t)tuya_mbuf_queue_get_used_size(chan->send_queue) : 0;
        }
        if (read_size != NULL) {
            *read_size = (chan->recv_queue != NULL) ? (uint32_t)tuya_mbuf_queue_get_used_size(chan->recv_queue) : 0;
        }
        if (send_free_size != NULL) {
            *send_free_size = (chan->send_queue != NULL) ? (uint32_t)tuya_mbuf_queue_get_free_size(chan->send_queue) : 0;
        }
    } else {
        ret = TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    tal_mutex_unlock(rtc->channel_lock);
    tal_mutex_unlock(g_p2p_session_mutex);
    return ret;
}

/**
 * @brief Recreate one channel KCP after releasing pending send/recv segments
 * @param[in] chan channel
 * @param[in] conv KCP conversation id
 * @return 0 on success, -1 on failure
 */
static int __rtc_channel_recreate_kcp(rtc_channel_t *chan, uint32_t conv)
{
    uint32_t send_wnd;
    uint32_t recv_wnd;
    uint32_t recv_cap;

    if (chan == NULL) {
        return -1;
    }
    if (chan->kcp != NULL) {
        ikcp_release(chan->kcp);
        chan->kcp = NULL;
    }
    chan->kcp = ikcp_create(conv, chan);
    if (chan->kcp == NULL) {
        return -1;
    }
    send_wnd = chan->send_buf_capacity / TUYA_MBUF_HUGE_SIZE;
    if (send_wnd == 0) {
        send_wnd = 1;
    }
    if (chan->channel_id < (int)TUYA_P2P_CHANNEL_NUMBER_MAX) {
        recv_cap = g_options.recv_buf_size[chan->channel_id];
    } else {
        recv_cap = chan->send_buf_capacity;
    }
    if (recv_cap == 0) {
        recv_cap = chan->send_buf_capacity;
    }
    recv_wnd = recv_cap / TUYA_MBUF_HUGE_SIZE;
    if (recv_wnd == 0) {
        recv_wnd = 1;
    }
    ikcp_setoutput(chan->kcp, on_kcp_output);
    ikcp_wndsize(chan->kcp, send_wnd, recv_wnd);
    ikcp_nodelay(chan->kcp, 0, 5, 20, g_options.preconnect_enable ? 0 : 1);
    ikcp_setmtu(chan->kcp, 1400);
    ikcp_setprocesspkt(chan->kcp, ctx_session_channel_process_pkt);
    return 0;
}

int32_t tuya_p2p_rtc_clear_send_buffer(int32_t handle, uint32_t channel_id)
{
    int32_t ret = 0;
    uint32_t used_before = 0;
    uint32_t used_after = 0;
    uint32_t free_after = 0;
    uint32_t conv;
    (void)handle;

    tal_mutex_lock(g_p2p_session_mutex);
    if (g_pRtcSession == NULL || g_pRtcSession->channels == NULL) {
        tal_mutex_unlock(g_p2p_session_mutex);
        return TUYA_P2P_ERROR_INVALID_SESSION_HANDLE;
    }
    if (channel_id > g_pRtcSession->cfg.channel_number) {
        tal_mutex_unlock(g_p2p_session_mutex);
        return TUYA_P2P_ERROR_INVALID_PARAMETER;
    }

    tal_mutex_lock(g_pRtcSession->channel_lock);
    {
        rtc_channel_t *chan = &g_pRtcSession->channels[channel_id];
        if (chan->send_queue != NULL) {
            used_before = (uint32_t)tuya_mbuf_queue_get_used_size(chan->send_queue);
        }
        if (channel_id == g_pRtcSession->cfg.channel_number) {
            conv = RTC_CHANNEL_CMD;
        } else {
            conv = channel_id;
        }
        if (__rtc_channel_recreate_kcp(chan, conv) != 0) {
            ret = TUYA_P2P_ERROR_OUT_OF_MEMORY;
        } else if (chan->send_queue != NULL) {
            used_after = (uint32_t)tuya_mbuf_queue_get_used_size(chan->send_queue);
            free_after = (uint32_t)tuya_mbuf_queue_get_free_size(chan->send_queue);
        }
    }
    tal_mutex_unlock(g_pRtcSession->channel_lock);
    tal_mutex_unlock(g_p2p_session_mutex);

    return ret;
}

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
    ctx_deinit_msg_queue_and_worker();
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "tal_log.h"
#include "tal_hash.h"
#include "tal_mutex.h"
#include "tal_system.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tuya_ipc_p2p.h"
#include "tuya_ipc_p2p_error.h"
#include "tuya_ipc_p2p_inner.h"
#include "tuya_ipc_p2p_common.h"
#include "tuya_ipc_media_stream_event.h"
#include "tuya_ipc_media_stream.h"
#include "tuya_ipc_media_adapter.h"
#include "tuya_media_service_rtc.h"
#include "rtp-payload.h"
#include "rtp-packet.h"

#define TUYA_CMD_CHANNEL        (0) // Signaling channel, signal mode refer to P2P_CMD_E
#define TUYA_VDATA_CHANNEL      (1) // Video data channel
#define TUYA_ADATA_CHANNEL      (2) // Audio data channel
#define TUYA_P2P_CMD_CHECK(cmd) (cmd == P2P_LIVE || cmd == P2P_PLAYBACK || cmd == P2P_PAUSE)
#define TUYA_P2P_CHN_CHECK(cmd) (cmd == TUYA_CMD_CHANNEL || cmd == TUYA_VDATA_CHANNEL || cmd == TUYA_ADATA_CHANNEL)

#define P2P_SESSION_IDLE    (0)
#define P2P_SESSION_RUNNING (1)
#define P2P_SESSION_CLOSING (2)

/* Tuya c2c intercom audio operations (sub-type of TY_C2C_CMD_IO_CTRL_AUDIO, high=8).
 * Align TuyaOS APP<->device speaker protocol. Adjust values per App cmd log if mismatched. */
typedef enum {
    TY_CMD_IO_CTRL_AUDIO_SPEAKER_START = 0,
    TY_CMD_IO_CTRL_AUDIO_SPEAKER_STOP = 1,
} TY_CMD_IO_CTRL_AUDIO_OP_E;

#define TUYA_IPC_P2P_DEFAULT_CAMERA (0)
#define P2P_RTP_PACK_LEN            (1100 + 128) // RTP packet buffer size
#define P2P_RECV_TIMEOUT            (30)

#define P2P_CHECK_USER_TIMES (10000) // 10s
// Password synchronization structure
typedef struct P2P_CMD_PASSWD_ {
    int mark;        // Custom identification mark
    int reqId;       // Client-defined request ID, used as unique identifier
    char user[32];   // Username
    char passwd[64]; // Password
} P2P_CMD_PASSWD_T;

// Control signal header structure
typedef struct P2P_CMD_PARSE_ {
    int mark;  // Custom identification mark
    int reqId; // Client-defined request ID, used as unique identifier
    C2C_CMD_FIXED_HEADER_T str_header;
} P2P_CMD_PARSE_T;

#define P2P_CMD_PARSE_MAX_SIZE_V2 (4096)
#define P2P_CMD_HEAD_LEN          (sizeof(P2P_CMD_PARSE_T))

#define MAX_PAYLOAD_SIZE (1100) /**MAX PAYLOAD SIZE*/
#define RTP_MTU_LEN      MAX_PAYLOAD_SIZE
#define RTP_SPLIT_LEN    RTP_MTU_LEN
#define TUYA_RTP_HEAD    0x12345678    // Custom RTP identification packet header
#define P2P_CMD_MARK     TUYA_RTP_HEAD // Temporarily reuse with RTP

#define READ_HEADER_PART  0 // Read header part
#define READ_PAYLOAD_PART 1 // Read payload part

#define EXT_PROTOCOL_V0_LEN (12)
#define P2P_EXT_HEAD_MAX_LEN                                                                                           \
    (sizeof(C2C_AV_TRANS_FIXED_HEADER) + EXT_PROTOCOL_V0_LEN) // Extended video header protocol V0 head+ext(8+4)+rtp_len

#define OFFSET(TYPE, MEMBER) ((size_t)(&(((TYPE *)0)->MEMBER)))

#define STACK_SIZE_P2P_MEDIA_SEND 98304
#define STACK_SIZE_P2P_MEDIA_RECV 65536
#define STACK_SIZE_P2P_CMD_SEND   65536
#define STACK_SIZE_P2P_CMD_RECV   65536
#define STACK_SIZE_P2P_DETECT     65536
#define STACK_SIZE_P2P_LISTEN     131072

typedef struct {
    int client;
    int channel;
    char *p_rtp_buff;                         // RTP data buffer, reference size MTU+100
    int fix_len;                              // Supplementary private header data
    /* Packets of the current frame already handed to the transport. Once this
     * is non-zero the frame is partly on the wire and cannot be retried. */
    int  sent_pkts;
    char ext_head_buff[P2P_EXT_HEAD_MAX_LEN]; // According to extended video header protocol head+ext(8)+rtp_len
} RTP_PACK_NAL_ARG_T;

typedef enum {
    P2P_IDLE = 0,
    P2P_VIDEO = 0x1, // Start live stream request
    P2P_AUDIO = 0x2,
    P2P_PB_VIDEO = 0x4, // Start playback request
    P2P_PB_AUDIO = 0x8,
    P2P_PB_PAUSE = 0x10, // Pause video request
    P2P_SPEAKER = 0x20,  // Intercom request
} P2P_CMD_E;

typedef struct {
    int read_size; // init P2P_CMD_HEAD_LEN;
    char read_buff[P2P_CMD_PARSE_MAX_SIZE_V2];
    int cur_read; // Current read length
    int flag;     // READ_HEADER_PART/READ_PAYLOAD_PART
} P2P_DATA_PARSE_T;

/* Rate control states per draft-ietf-rmcat-gcc-02. Hold prevents climbing
 * straight back into a queue that has not finished draining. */
typedef enum {
    RC_STATE_INCREASE = 0,
    RC_STATE_HOLD,
    RC_STATE_DECREASE,
} RC_STATE_E;

typedef struct {
    MUTEX_HANDLE cmutex;
    TUYA_IPC_P2P_AUTH_T str_P2p_auth;
    /*******client*******/
    int session; // Save session number
    int status;  // Session status  0 not started
    /*******p2p server*******/
    P2P_CMD_E cmd; // Signal status information
    P2P_CMD_PARSE_T pb_resp_head;
    char *p_video_rtp_buff; // Video RTP data buffer, reference size MTU+100
    char *p_audio_rtp_buff; // Audio RTP data buffer, reference size MTU+100
    uint16_t video_seq_num;   // Video RTP packet sequence number
    uint16_t audio_seq_num;   // Audio RTP packet sequence number
    BOOL_T key_frame;
    BOOL_T video_need_iframe;                        // Wait for first I-frame after video start
    uint64_t v_pts;                                  // Video PTS
    uint64_t v_timestamp;                            // Video absolute time (ms)
    uint64_t a_pts;                                  // Audio PTS
    uint64_t a_timestamp;                            // Audio absolute time (ms)
    int video_req_id;                              // Video request ID, used for preview, playback and other services
    int audio_req_id;                              // Audio (mic uplink) request ID
    int speak_req_id;                              // Speaker (downlink intercom) request ID, align OS
    TRANSFER_VIDEO_CLARITY_TYPE_INNER_E cur_clarity; // Current video clarity type
    P2P_DATA_PARSE_T proto_parse;
    TRANS_IPC_AV_INFO_T av_Info; // TODO currently video parameters must be consistent
    /* Speaker decode params for QUERY_AUDIO_PARAMS (align OS audio_decode_info) */
    BOOL_T speak_audio_enable;
    TY_AV_CODEC_ID speak_audio_codec;
    TRANSFER_AUDIO_SAMPLE_E speak_audio_sample;
    TRANSFER_AUDIO_DATABITS_E speak_audio_databits;
    TRANSFER_AUDIO_CHANNEL_E speak_audio_channel;
    uint32_t dbg_vsend_ok;                             // DBG: successful video RTP sends
    uint32_t dbg_vsend_skip;                           // DBG: skipped non-I before first key
    uint32_t dbg_vget_fail;                            // DBG: get-frame callback failures
    uint32_t                  dbg_vsend_fail;                           // video sends that returned an error
    uint32_t                  dbg_asend_fail;                           // audio sends that returned an error
    uint32_t                  dbg_ashed;                                // audio frames dropped past the deadline
    uint32_t                  dbg_aq_used;                              // bytes the audio channel last had in flight

    /* Rate control: how full the video transport queue is, and the bitrate the
     * encoder has been told to produce as a result. See __p2p_rate_control. */
    uint32_t   tx_fill_pct;                             // occupancy of the video send queue, 0-100
    uint32_t   tx_full_cnt;                             // times a frame did not fit, this session
    uint32_t   tx_max_frame;                            // largest frame offered, sizes the queue floor
    uint32_t   tx_drop_cnt;                             // key frames that shed a stale backlog
    uint32_t   rc_fill_peak;                            // worst occupancy seen in the current window
    uint32_t   rc_fill_sum;                             // occupancy accumulated over the window
    uint32_t   rc_fill_samples;                         // samples behind rc_fill_sum
    uint32_t   rc_full_at_start;                        // tx_full_cnt when the window opened
    uint32_t   rc_warmup;                               // windows skipped while the link settles
    RC_STATE_E rc_state;                                // increase / hold / decrease, see the draft
    uint64_t   rc_window_ms;                            // when the current window opened
    uint32_t   rc_bw_kbps;                              // smoothed link capacity the transport measured
    uint32_t   rc_base_kbps;                            // configured rate, the ceiling to return to
    uint32_t   rc_cur_kbps;                             // rate currently commanded
    uint64_t   iframe_req_ms;                           // last key frame request, to rate limit them
    BOOL_T video_frame_pending;                     // Hold last get_frame until RTP send succeeds

    tuya_p2p_rtc_disconnect_cb_t on_disconnect_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_video_frame_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_audio_frame_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_video_start_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_video_stop_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_audio_start_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_audio_stop_callback;
    tuya_p2p_rtc_get_frame_cb_t  on_recv_audio_frame_callback;
    tuya_p2p_rtc_req_i_frame_cb_t on_request_i_frame_callback;
    tuya_p2p_rtc_set_bitrate_cb_t on_set_video_bitrate_callback;
    THREAD_HANDLE cmd_recv_proc_thread;   // Command receive thread handle
    THREAD_HANDLE video_send_proc_thread; // Video send thread handle
    THREAD_HANDLE audio_downlink_thread;  // Downlink audio (APP->spk) recv thread
    BOOL_T audio_downlink_on;             // Downlink audio recv loop flag
    // TAL_VENC_FRAME_T tal_video_frame;
    // TAL_AUDIO_FRAME_INFO_T tal_audio_frame;
    MEDIA_FRAME media_frame;
    MEDIA_FRAME media_audio_frame;
    /******* p2p server*******/
} P2P_SESSION_T;

static P2P_SESSION_T *sg_p2p_session = NULL;
int g_listen_start = 0;               // Flag variable to control listen thread start or stop
THREAD_HANDLE g_listen_thrd_hdl = NULL; // Listen thread handle

OPERATE_RET p2p_deal_with_listen(int session);
OPERATE_RET p2p_get_userinfo(int session, int p2pType);
IPC_STREAM_TYPE p2p_get_chn_idx(TRANSFER_VIDEO_CLARITY_TYPE_INNER_E cur_clarity);
TRANSFER_VIDEO_CLARITY_TYPE p2p_clarity_trans(TRANSFER_VIDEO_CLARITY_TYPE_INNER_E type);
int p2p_prepare_video_send_resource(P2P_SESSION_T *pSession);
int p2p_release_video_send_resource(P2P_SESSION_T *pSession);
int p2p_prepare_audio_send_resource(P2P_SESSION_T *pSession);
int p2p_release_audio_send_resource(P2P_SESSION_T *pSession);
int __p2p_session_clear(P2P_SESSION_T *pSession);
int __p2p_session_all_stop(P2P_SESSION_T *pSession);
int __p2p_session_release_va(P2P_SESSION_T *pSession);
void __p2p_thread_exit(THREAD_HANDLE thread);
void __p2p_rtc_close(int rtc_session, int reason, P2P_SESSION_T* p2p_session);

void *rtp_alloc(void *param, int bytes);
void rtp_free(void *param, void *packet);
int rtp_pack_packet_handler(void *param, const void *packet, int bytes, uint32_t timestamp, int flags);

void ctx_listen_thread_func(void *arg)
{
    (void)arg;
    PR_NOTICE("p2p_listen");
    while (1) {
        int session_id;
        session_id = tuya_p2p_rtc_listen();
        if (session_id < 0) {
            PR_ERR("p2p listen failed session:[%d]", session_id);
            break;
        }
        PR_NOTICE("__p2p_deal_with_listen, session[%d]", session_id);
        p2p_deal_with_listen(session_id);
    }
    /* p2p_rtc_listen_start checks this flag; leaving it set after the loop ends
     * means the thread never restarts. */
    g_listen_start = 0;
    PR_ERR("p2p listen task exit - no further peers will be accepted until listen is restarted");
    return;
}

OPERATE_RET p2p_rtc_listen_start()
{
    THREAD_CFG_T param = {0};
    param.priority = THREAD_PRIO_3;
    param.stackDepth = 128 * 1024;
    param.thrdname = "tuya_p2p_listen_task";
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    param.psram_mode = 1; /* Align OS tuya_p2p_lib_pthread_create → PSRAM stack */
#endif
    if (g_listen_start) {
        PR_ERR("p2p listen thread already started");
        return OPRT_COM_ERROR;
    }
    int result = tal_thread_create_and_start(&g_listen_thrd_hdl, NULL, NULL, ctx_listen_thread_func, NULL, &param);
    if (OPRT_OK != result) {
        PR_ERR("create p2p listen thread failed %d", result);
        return result;
    }
    g_listen_start = 1;
    return OPRT_OK;
}

OPERATE_RET p2p_rtc_listen_stop()
{
    if (g_listen_start != 1) {
        PR_ERR("p2p listen thread not started");
        return OPRT_COM_ERROR;
    }
    tuya_p2p_rtc_listen_break();
    tal_thread_delete(g_listen_thrd_hdl);
    g_listen_start = 0;
    return OPRT_OK;
}

OPERATE_RET p2p_deal_with_listen(int session)
{
    OPERATE_RET ret = OPRT_OK;
    BOOL_T userCheckEnable = FALSE;

    PR_NOTICE("__p2p_deal_with_listen, session[%d]", session);

    // Every branch below dereferences the session context, so refuse the
    // connection instead of faulting if it is not up yet.
    if (NULL == sg_p2p_session) {
        PR_ERR("p2p session not initialized, reject session[%d]", session);
        __p2p_rtc_close(session, RTC_CLOSE_REASON_SESSION_FULL, NULL);
        return OPRT_COM_ERROR;
    }

    /* One session context means one peer; a second caller would overwrite the
     * first while its threads are still reading it. */
    if (P2P_SESSION_IDLE != sg_p2p_session->status) {
        PR_WARN("session[%d] already active (status %d), reject session[%d]", sg_p2p_session->session,
                sg_p2p_session->status, session);
        __p2p_rtc_close(session, RTC_CLOSE_REASON_SESSION_FULL, NULL);
        return OPRT_COM_ERROR;
    }

    // First verify user information, close corresponding session if not qualified
    if (OPRT_OK != p2p_get_userinfo(session, 1)) {
        PR_ERR("get userinfo error session[%d]", session);
        if (FALSE == userCheckEnable) {
            PR_ERR("resend p2p passwd to service");
            if (OPRT_OK == tuya_ipc_p2p_update_pw(sg_p2p_session->str_P2p_auth.p2p_passwd)) {
                userCheckEnable = TRUE;
            }
        }
        PR_ERR("Close session[%d]", session);
        // Close just this session. Tearing the RTC stack down here would
        // destroy the global message queue, worker and session mutex, so one
        // rejected peer would leave P2P dead until the process restarts.
        __p2p_rtc_close(session, RTC_CLOSE_REASON_AUTH_FAIL, NULL);
        return OPRT_COM_ERROR;
    } else {
        // Once verification is successful, no more authentication exception handling
        userCheckEnable = TRUE;
    }

    // Request session-related resources
    if (OPRT_OK != (ret = p2p_prepare_video_send_resource(sg_p2p_session))) {
        PR_ERR("session[%d] open_stream video failed", session);
        goto RET;
    }
    if (OPRT_OK != (ret = p2p_prepare_audio_send_resource(sg_p2p_session))) {
        PR_ERR("session[%d] open_stream audio failed", session);
        goto RET;
    }

    // Save connection information
    sg_p2p_session->session = session;
    sg_p2p_session->status = P2P_SESSION_RUNNING;
    PR_NOTICE("create p2p sessions. cur online session num = %d", 1);
    return OPRT_OK;

RET:
    /* The peer is connected as far as the RTC layer is concerned, so leaving
     * without closing it strands the session: it never times out here and the
     * App waits on a stream that will never start. */
    __p2p_rtc_close(session, RTC_CLOSE_REASON_SESSION_FULL, NULL);
    return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  Function: __p2p_get_passwd
 *  Note:Session listening thread, start corresponding session thread when there is session connection
 *  Input: session session number
 *  Output: none
 *  Return:
 ***********************************************************/
OPERATE_RET p2p_get_userinfo(int session, int p2pType)
{
    char *read_buff = NULL;
    P2P_CMD_PASSWD_T strUserInfo;
    int ret;
    int cur_read = 0;
    int32_t read_size = (int32_t)sizeof(P2P_CMD_PASSWD_T);
    int32_t tmpSize = 0;
    BOOL_T flag = FALSE;
    int timeout = P2P_RECV_TIMEOUT; // ms
    /* P2P_CHECK_USER_TIMES already reads 10s; the extra factor of six stretched
     * the handshake timeout to a minute. */
    int retry      = P2P_CHECK_USER_TIMES / timeout;
    int total_read = 0;

    memset(&strUserInfo, 0x00, sizeof(P2P_CMD_PASSWD_T));
    read_buff = (char *)&strUserInfo;

    while (retry > 0) {
        retry--;
        tmpSize = read_size;
        ret = tuya_p2p_rtc_recv_data(session, TUYA_CMD_CHANNEL, read_buff + cur_read, &read_size, timeout);
        if ((ret < 0) && (ERROR_P2P_TIME_OUT != ret)) {
            // Exception handling
            if (ERROR_P2P_SESSION_CLOSED_REMOTE == ret || ERROR_P2P_SESSION_CLOSED_TIMEOUT == ret ||
                ERROR_P2P_SESSION_CLOSED_CALLED == ret) {
                // Session was closed by client, need to close session
                PR_ERR("session[%d] was close by client ret[%d]", session, ret);
                return OPRT_COM_ERROR;
            } else {
                // Other exceptions to be supplemented later
            }
            // Not read, restore value
            read_size = tmpSize;
        } else {
            if (sizeof(P2P_CMD_PASSWD_T) == (read_size + cur_read)) {
                // Complete user information obtained, perform simple mark verification
                uint32_t mark_raw = (uint32_t)((P2P_CMD_PASSWD_T *)read_buff)->mark;
                uint32_t mark_swap =
                    ((mark_raw & 0x000000FFU) << 24) | ((mark_raw & 0x0000FF00U) << 8) |
                    ((mark_raw & 0x00FF0000U) >> 8) | ((mark_raw & 0xFF000000U) >> 24);
                {
                    uint8_t *b = (uint8_t *)read_buff;
                }
                if (P2P_CMD_MARK != mark_raw && P2P_CMD_MARK != mark_swap) {
                    // Header parsing exception, exception handling to be completed later (unlikely to reach this
                    // condition)
                    PR_ERR("session[%d] read data error mark[0x%x]", session, mark_raw);
                    return OPRT_COM_ERROR;
                }
                if (P2P_CMD_MARK == mark_swap && P2P_CMD_MARK != mark_raw) {
                    uint32_t req_raw = (uint32_t)((P2P_CMD_PASSWD_T *)read_buff)->reqId;
                    uint32_t req_swap =
                        ((req_raw & 0x000000FFU) << 24) | ((req_raw & 0x0000FF00U) << 8) |
                        ((req_raw & 0x00FF0000U) >> 8) | ((req_raw & 0xFF000000U) >> 24);
                    ((P2P_CMD_PASSWD_T *)read_buff)->mark = (int)mark_swap;
                    ((P2P_CMD_PASSWD_T *)read_buff)->reqId = (int)req_swap;
                }
                flag = TRUE;
                break;
            } else if (sizeof(P2P_CMD_PASSWD_T) > (read_size + cur_read)) {
                total_read += read_size;
                cur_read += read_size;
                read_size = sizeof(P2P_CMD_PASSWD_T) - cur_read;
            } else {
                PR_ERR("get userinfo error session[%d]", session);
                return OPRT_COM_ERROR;
            }
        }
    } // while (retry > 0)

    if (FALSE == flag) {
        /* Say whether the peer was silent or just incomplete: ICE can report
         * success while no payload ever crosses, and the two look identical
         * from the app side. */
        PR_ERR("get userinfo timeout session[%d]: %d of %d bytes arrived in %dms", session, total_read,
               (int)sizeof(P2P_CMD_PASSWD_T), P2P_CHECK_USER_TIMES);
        return OPRT_COM_ERROR;
    }

    PR_DEBUG("compare passwd");
    char sign[32 + 1] = {0};
    TKL_HASH_HANDLE md5;
    tal_md5_create_init(&md5);
    tal_md5_starts_ret(md5);
    unsigned char decrypt[16];
    tal_md5_update_ret(md5, (uint8_t *)(sg_p2p_session->str_P2p_auth.p2p_passwd),
                       strlen(sg_p2p_session->str_P2p_auth.p2p_passwd));
    tal_md5_update_ret(md5, (uint8_t *)"||", 2);
    tal_md5_update_ret(md5, (uint8_t *)(sg_p2p_session->str_P2p_auth.gw_local_key),
                       strlen(sg_p2p_session->str_P2p_auth.gw_local_key));
    tal_md5_finish_ret(md5, decrypt);
    tal_md5_free(md5);

    int offset = 0;
    int i = 0;
    for (i = 0; i < 16; i++) {
        sprintf(&sign[offset], "%02x", decrypt[i]);
        offset += 2;
    }
    sign[offset] = 0;

    if (strcmp(strUserInfo.user, sg_p2p_session->str_P2p_auth.p2p_name) == 0 && strcmp(strUserInfo.passwd, sign) == 0) {
        PR_DEBUG("auth success");
        return OPRT_OK;
    }

    char lk_dm5[32 + 1] = {0};
    tal_md5_create_init(&md5);
    tal_md5_starts_ret(md5);
    tal_md5_update_ret(md5, (uint8_t *)(sg_p2p_session->str_P2p_auth.gw_local_key),
                       strlen(sg_p2p_session->str_P2p_auth.gw_local_key));
    tal_md5_finish_ret(md5, decrypt);
    tal_md5_free(md5);
    offset = 0;
    for (i = 0; i < 16; i++) {
        sprintf(&lk_dm5[offset], "%02x", decrypt[i]);
        offset += 2;
    }
    lk_dm5[offset] = 0;
    // PR_DEBUG("Client Auth %s %s <-> %s %s ", strUserInfo.user, strUserInfo.passwd, sg_p2p_ctl.str_P2p_auth.p2p_name,
    // sg_p2p_ctl.str_P2p_auth.p2p_passwd);
    PR_DEBUG("localkey md5:%s final:%s", lk_dm5, sign);

    PR_ERR("auth failed");

    return OPRT_COM_ERROR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void __p2p_thread_exit(THREAD_HANDLE thread)
{
    if (NULL != thread) {
        tal_thread_delete(thread);
    }
    return;
}

void __p2p_rtc_close(int rtc_session, int reason, P2P_SESSION_T* p2p_session)
{
    tuya_p2p_rtc_close(rtc_session, reason);
    return;
}

IPC_STREAM_TYPE p2p_get_chn_idx(TRANSFER_VIDEO_CLARITY_TYPE_INNER_E cur_clarity)
{
    IPC_STREAM_TYPE chn = eIpcStreamVideoMain;
    TRANSFER_VIDEO_CLARITY_TYPE type = p2p_clarity_trans(cur_clarity);

    switch (type) {
    case eVideoClarityStandard:
        chn = eIpcStreamVideoSub;
        break;
    case eVideoClarityHigh:
        chn = eIpcStreamVideoMain;
        break;
    case eVideoClarityThird:
        chn = eIpcStreamVideo3rd;
        break;
    case eVideoClarityFourth:
        chn = eIpcStreamVideo4th;
        break;
    default:
        chn = eIpcStreamVideoMain;
        break;
    }

    return chn;
}

TRANSFER_VIDEO_CLARITY_TYPE p2p_clarity_trans(TRANSFER_VIDEO_CLARITY_TYPE_INNER_E type)
{
    if (TY_VIDEO_CLARITY_INNER_STANDARD == type) {
        return eVideoClarityStandard;
    } else if (TY_VIDEO_CLARITY_INNER_HIGH == type) {
        return eVideoClarityHigh;
    }
    return eVideoClarityHigh;
}

int p2p_prepare_video_send_resource(P2P_SESSION_T *pSession)
{
    if (pSession == NULL) {
        PR_DEBUG("session is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL != pSession->p_video_rtp_buff) {
        return OPRT_OK;
    }

    pSession->p_video_rtp_buff = (char *)Malloc(P2P_RTP_PACK_LEN);
    if (NULL == pSession->p_video_rtp_buff) {
        PR_ERR("session:[%d] video rtp buffer malloc failed", pSession->session);
        return OPRT_MALLOC_FAILED;
    }
    memset(pSession->p_video_rtp_buff, 0x00, P2P_RTP_PACK_LEN);

    PR_DEBUG("session:[%d] malloc video send buffer success", pSession->session);
    return OPRT_OK;
}

int p2p_release_video_send_resource(P2P_SESSION_T *pSession)
{
    if (pSession == NULL) {
        PR_DEBUG("session is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL == pSession->p_video_rtp_buff) {
        return OPRT_OK;
    }

    Free(pSession->p_video_rtp_buff);
    pSession->p_video_rtp_buff = NULL;

    PR_DEBUG("session:[%d] release video send buffer success", pSession->session);
    return OPRT_OK;
}

int p2p_prepare_audio_send_resource(P2P_SESSION_T *pSession)
{
    if (pSession == NULL) {
        PR_DEBUG("session is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL != pSession->p_audio_rtp_buff) {
        return OPRT_OK;
    }

    pSession->p_audio_rtp_buff = (char *)Malloc(P2P_RTP_PACK_LEN);
    if (NULL == pSession->p_audio_rtp_buff) {
        PR_ERR("session:[%d] audio rtp buffer malloc failed", pSession->session);
        return OPRT_MALLOC_FAILED;
    }
    memset(pSession->p_audio_rtp_buff, 0x00, P2P_RTP_PACK_LEN);

    PR_DEBUG("session:[%d] malloc audio send buffer success", pSession->session);
    return OPRT_OK;
}

int p2p_release_audio_send_resource(P2P_SESSION_T *pSession)
{
    if (pSession == NULL) {
        PR_DEBUG("session is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL == pSession->p_audio_rtp_buff) {
        return OPRT_OK;
    }

    Free(pSession->p_audio_rtp_buff);
    pSession->p_audio_rtp_buff = NULL;

    PR_DEBUG("session:[%d] release audio send buffer success", pSession->session);
    return OPRT_OK;
}

OPERATE_RET p2p_send_rtp_data(int client, int channel, char *buff, int length)
{
    if (channel < TUYA_VDATA_CHANNEL || channel > TUYA_ADATA_CHANNEL) {
        PR_ERR("input errorclient[%d]channel[%d]", client, channel);
        return OPRT_INVALID_PARM;
    }
    int ret = 0;
    // Send data
    if ((0 == (P2P_VIDEO & sg_p2p_session->cmd)) && (0 == (P2P_PB_VIDEO & sg_p2p_session->cmd)) &&
        (0 == (P2P_AUDIO & sg_p2p_session->cmd)) && (0 == (P2P_PB_AUDIO & sg_p2p_session->cmd))) {
        return OPRT_OK;
    }
    ret = tuya_p2p_rtc_send_data(sg_p2p_session->session, channel, buff, length, -1);
    if (ret == length) {
        return OPRT_OK;
    }
    PR_ERR("Write data failed [%d][%d]", ret, length);
    if (ret == TUYA_P2P_ERROR_OUT_OF_MEMORY || ret == OPRT_RESOURCE_NOT_READY) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (ret > 0 && ret < length) {
        /* Part of this RTP packet is already on the wire, so a retry would duplicate
         * the sent half. Fail the frame instead. */
        PR_ERR("partial write %d of %d on channel %d, frame abandoned", ret, length, channel);
        return OPRT_COM_ERROR;
    }
    return OPRT_COM_ERROR;
}

/***********************************************************
 *  Function: __p2p_ext_protocol_pack
 *  Note:Transport extension protocol packet assembly
 *  Input: client channel number, pResult result buffer, type 0/1 video/audio
 *  Output: pResultLen result buffer size
 *  Return:
 ***********************************************************/
static void __p2p_ext_protocol_pack(int client, int type, char *p_result, int *p_result_len)
{
    if (NULL == p_result || NULL == p_result_len) {
        PR_ERR("input error");
        return;
    }

    int fix_len = 0; // 20180428 supplementary header data
    uint64_t tmpTime;
    int ipcChan = client;
    IPC_STREAM_E curClirtyChn = p2p_get_chn_idx(sg_p2p_session->cur_clarity);
    C2C_AV_TRANS_FIXED_HEADER *pav_Info = (C2C_AV_TRANS_FIXED_HEADER *)p_result;

    if (0 == type) {
        tmpTime = sg_p2p_session->v_timestamp;
        pav_Info->request_id = sg_p2p_session->video_req_id;
        if (TRUE == sg_p2p_session->key_frame) {
            fix_len = sizeof(C2C_AV_TRANS_FIXED_HEADER) + EXT_PROTOCOL_V0_LEN;
            pav_Info->extension_length = 8;
            *(uint8_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER)] = TY_EXT_VIDEO_PARAM;
            *(uint8_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 1] = 0;
            *(int16_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 2] =
                (int16_t)sg_p2p_session->av_Info.width[curClirtyChn];
            *(int16_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 4] =
                (int16_t)sg_p2p_session->av_Info.height[curClirtyChn];
            *(int16_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 6] =
                (int16_t)sg_p2p_session->av_Info.fps[curClirtyChn];
            /* DBG: what geometry the App is actually told, per I-frame. */
            PR_DEBUG("DBG video ext-header chn=%d w=%u h=%u fps=%u", curClirtyChn,
                     sg_p2p_session->av_Info.width[curClirtyChn], sg_p2p_session->av_Info.height[curClirtyChn],
                     sg_p2p_session->av_Info.fps[curClirtyChn]);
        } else {
            fix_len = sizeof(C2C_AV_TRANS_FIXED_HEADER) + 4;
            pav_Info->extension_length = 0;
        }
    } else {
        tmpTime = sg_p2p_session->a_timestamp;
        pav_Info->request_id = sg_p2p_session->audio_req_id;
        fix_len = sizeof(C2C_AV_TRANS_FIXED_HEADER) + EXT_PROTOCOL_V0_LEN;
        pav_Info->extension_length = 8;
        *(uint8_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER)] = TY_EXT_AUDIO_PARAM;
        *(uint8_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 1] = 0;
        *(int16_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 2] = (int16_t)sg_p2p_session->av_Info.audio_sample;
        *(int16_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 4] = (int16_t)sg_p2p_session->av_Info.audio_channel;
        *(int16_t *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 6] = (int16_t)sg_p2p_session->av_Info.audio_databits;
    }
    pav_Info->time_ms = tmpTime;
    *p_result_len = fix_len;

    return;
}

/* Video allowed to wait in the transport, as playout time rather than bytes:
 * a byte budget means a different delay at every bitrate. */
#define P2P_TX_LATENCY_BUDGET_MS 700u

/* Multiples of the largest frame seen that the queue must always accept, so
 * the budget can never shrink below what one key frame needs. */
#define P2P_TX_KEYFRAME_ROOM 3u

/**
 * @brief Recompute how full the video send queue is against the latency budget
 *
 * Kept apart from the buffer check because that check only runs when a frame is
 * actually offered, and while the sender is discarding frames waiting for a key
 * frame it offers none. Left unpolled the occupancy stays frozen at whatever
 * tripped the discard - always above the drain threshold, so the key frame
 * request that ends the discard never fires and the stream waits out the whole
 * GOP instead: measured 35 pct of encoded frames never leaving the device.
 *
 * @param[in]  len frame about to be offered, 0 when only polling
 * @param[out] write_size backlog in bytes, may be NULL
 * @param[out] send_free_size room left in the send queue, may be NULL
 * @return occupancy in percent, above 100 when over budget, -1 if unavailable
 */
static int __p2p_video_fill_pct(int len, int *write_size, int *send_free_size)
{
    uint32_t used = 0;
    uint32_t kbps;
    int      budget, keyframe_room, pct;

    if (sg_p2p_session == NULL) {
        return -1;
    }
    if (OPRT_OK != tuya_p2p_rtc_check_buffer(sg_p2p_session->session, TUYA_VDATA_CHANNEL, &used, NULL,
                                             (uint32_t *)send_free_size)) {
        return -1;
    }
    if (write_size != NULL) {
        *write_size = (int)used;
    }

    if ((uint32_t)len > sg_p2p_session->tx_max_frame) {
        sg_p2p_session->tx_max_frame = (uint32_t)len;
    }

    kbps = sg_p2p_session->rc_cur_kbps ? sg_p2p_session->rc_cur_kbps : 1024u;
    /* kbps * ms / 8 == bytes of playout time */
    budget = (int)((kbps * P2P_TX_LATENCY_BUDGET_MS) / 8u);
    /* A budget below one key frame reads as permanent congestion. */
    keyframe_room = (int)(sg_p2p_session->tx_max_frame * P2P_TX_KEYFRAME_ROOM);
    if (budget < keyframe_room) {
        budget = keyframe_room;
    }
    if (budget < 1) {
        budget = 1;
    }

    pct = (int)((int64_t)used * 100 / budget);
    sg_p2p_session->tx_fill_pct = (pct > 100) ? 100u : (uint32_t)pct;
    return pct;
}

/*
 * Longest the audio queue may run ahead of the far end before frames are
 * dropped rather than queued. KCP delivers everything it accepts, in order, so
 * a frame handed over here will be played however stale it has become, and
 * this queue is the one part of the mouth-to-ear delay the sending side still
 * decides. Left unbounded it does not settle anywhere useful: measured on
 * hardware, the audio channel reached 231 outstanding packets - 6.2 seconds of
 * speech nobody would still want to hear - and then drained them at two and a
 * half times its own rate, holding video to a tenth of its throughput for the
 * fifteen seconds that took.
 *
 * The budget is spent on two things, not one. What KCP is waiting to send is
 * the queue this exists to bound, but what it has sent and not had acked is
 * counted too, and that part is the round trip rather than any backlog: at the
 * 180 ms this link runs at, four frames are gone before a queue forms at all.
 * Leave room for both, or the valve opens on an idle channel - measured at
 * 500 ms, it shed 26 frames in a quiet 100 seconds with nothing congested.
 */
#define P2P_AUDIO_LATENCY_BUDGET_MS 900u

/* Each frame carries a P2P header into KCP as well as its samples, and the
 * queue is measured in what KCP holds, so the budget has to be too. */
#define P2P_AUDIO_FRAME_OVERHEAD_PCT 15u

/**
 * @brief Bytes of audio the codec produces in a second
 */
static uint32_t __p2p_audio_byte_rate(void)
{
    static const uint32_t hz[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
    uint32_t              rate, width = 1;
    TRANS_IPC_AV_INFO_T  *info = &sg_p2p_session->av_Info;

    if ((unsigned)info->audio_sample >= sizeof(hz) / sizeof(hz[0])) {
        return hz[0];
    }
    rate = hz[info->audio_sample];
    /* G.711 is a byte a sample whatever the frame says its width is. */
    if (TY_AV_CODEC_AUDIO_PCM == info->audio_codec) {
        width = (TY_AUDIO_DATABITS_16 == info->audio_databits) ? 2u : 1u;
        width *= (TY_AUDIO_CHANNEL_STERO == info->audio_channel) ? 2u : 1u;
    }
    return rate * width;
}

/**
 * @brief Has the audio queue run further ahead than the budget allows?
 */
static BOOL_T __p2p_audio_over_budget(void)
{
    uint32_t used = 0;
    uint32_t budget;

    if (sg_p2p_session == NULL) {
        return FALSE;
    }
    /* waitsnd counts what KCP holds both unsent and unacked, which is the whole
     * of what stands between this frame and the speaker. */
    if (OPRT_OK != tuya_p2p_rtc_check_buffer(sg_p2p_session->session, TUYA_ADATA_CHANNEL, &used, NULL, NULL)) {
        return FALSE;
    }
    sg_p2p_session->dbg_aq_used = used;
    budget = (__p2p_audio_byte_rate() * P2P_AUDIO_LATENCY_BUDGET_MS) / 1000u;
    budget += (budget * P2P_AUDIO_FRAME_OVERHEAD_PCT) / 100u;
    return (used > budget) ? TRUE : FALSE;
}

void tuya_ipc_p2p_audio_stats_get(uint32_t *shed, uint32_t *send_fail, uint32_t *queued)
{
    P2P_SESSION_T *pSession = sg_p2p_session;

    if (shed) {
        *shed = pSession ? pSession->dbg_ashed : 0;
    }
    if (send_fail) {
        *send_fail = pSession ? pSession->dbg_asend_fail : 0;
    }
    if (queued) {
        *queued = pSession ? pSession->dbg_aq_used : 0;
    }
}

/**
 * @brief May another frame of @p len be queued without falling too far behind?
 *
 * Also records how full the video queue is relative to that latency budget,
 * which is the congestion signal the encoder bitrate is driven from.
 */
static OPERATE_RET __p2p_check_free_buffer_size(int client, int channel, int len)
{
    OPERATE_RET ret = OPRT_OK;
    int sendFreeSize = 0;
    int writeSize = 0;

    (void)client; /* one session, one transport - the id comes from the context */

    if (channel == TUYA_VDATA_CHANNEL) {
        int pct = __p2p_video_fill_pct(len, &writeSize, &sendFreeSize);

        if (pct < 0) {
            return OPRT_COM_ERROR;
        }
        /* Judged on the existing backlog, not on whether this frame also fits:
         * a key frame can exceed the budget alone and must still get through. */
        if (pct > 100) {
            if ((sg_p2p_session->tx_full_cnt % 100) == 0) {
                PR_WARN("video queue %d bytes is %d pct of the %ums budget, shedding frames", writeSize, pct,
                        P2P_TX_LATENCY_BUDGET_MS);
            }
            sg_p2p_session->tx_full_cnt++;
            return OPRT_RESOURCE_NOT_READY;
        }
    } else {
        ret = tuya_p2p_rtc_check_buffer(sg_p2p_session->session, channel, (uint32_t *)&writeSize, NULL,
                                        (uint32_t *)&sendFreeSize);
        if (OPRT_OK != ret) {
            return ret;
        }
    }

    int need_size = (int)((double)len * 1.1); /* align TuyaOS svc_streaming_p2p check */
    if (need_size > sendFreeSize) {
        /* Per-session, not per-process: a function-level static would share one
         * counter between the video and audio channels and never reset across
         * connections, so the rate limit fires at unrelated moments. */
        if ((sg_p2p_session->tx_full_cnt % 100) == 0) {
            PR_ERR("Check_Buffer not enough writeSize[%d] sendFreeSize[%d] len[%d] session[%d] channel[%d]", writeSize,
                   sendFreeSize, len, sg_p2p_session->session, channel);
        }
        sg_p2p_session->tx_full_cnt++;
        ret = OPRT_RESOURCE_NOT_READY;
    }
    return ret;
}

/* Encoder rate control driven by the transport: the link decides what the
 * encoder may produce, not the other way round. */
/* Constants follow draft-ietf-rmcat-gcc-02. */
#define RC_WINDOW_MS       1000u /* one decision per second: slower than the queue moves */
#define RC_FILL_HIGH_PCT   70u   /* sustained, not a momentary spike */
#define RC_FILL_LOW_PCT    20u
#define RC_DOWN_NUM        85u /* x0.85 on congestion, per the draft */
#define RC_DOWN_DEN        100u
#define RC_UP_NUM          108u /* x1.08 per second while the link is clear */
#define RC_UP_DEN          100u
#define RC_MIN_PCT         25u /* never fall below a quarter of the configured rate */
#define RC_FILL_EVENTS_MIN 3u  /* shed frames in a window before believing congestion */

/* Share of the measured link the encoder may ask for; the rest covers KCP
 * headers, audio, retransmissions and measurement lag. */
#define RC_BW_SHARE_NUM 85u
#define RC_BW_SHARE_DEN 100u

/* Smoothing on the measurement: the raw estimate swings hard on a poor link
 * and re-keying the encoder that often costs more than the tracking is worth. */
#define RC_BW_SMOOTH_OLD 3u
#define RC_BW_SMOOTH_DEN 4u

/* Smallest change worth reprogramming the encoder for: every reconfiguration
 * emits a fresh key frame. */
#define RC_MIN_CHANGE_PCT 5u
/* Windows to let pass before acting: a session opens with a cold KCP window
 * and a key frame, so the queue is briefly deep through no fault of the encoder. */
#define RC_WARMUP_WINDOWS 2u

static void __p2p_rate_control(P2P_SESSION_T *pSession)
{
    uint64_t now = (uint64_t)tal_system_get_millisecond();
    uint32_t fills, want, floor_kbps, avg_fill;

    if (pSession->on_set_video_bitrate_callback == NULL || pSession->rc_base_kbps == 0) {
        return;
    }
    /* Averaged over the window, not peaked: a single deep moment is normal
     * around a key frame and says nothing about what the link can carry. */
    pSession->rc_fill_sum += pSession->tx_fill_pct;
    pSession->rc_fill_samples++;
    if (pSession->tx_fill_pct > pSession->rc_fill_peak) {
        pSession->rc_fill_peak = pSession->tx_fill_pct;
    }
    if (pSession->rc_window_ms == 0) {
        pSession->rc_window_ms     = now;
        pSession->rc_full_at_start = pSession->tx_full_cnt;
        return;
    }
    if (now - pSession->rc_window_ms < RC_WINDOW_MS) {
        return;
    }

    fills      = pSession->tx_full_cnt - pSession->rc_full_at_start;
    avg_fill   = pSession->rc_fill_samples ? (pSession->rc_fill_sum / pSession->rc_fill_samples) : 0;
    want       = pSession->rc_cur_kbps;
    floor_kbps = pSession->rc_base_kbps * RC_MIN_PCT / 100u;
    if (floor_kbps == 0) {
        floor_kbps = 1;
    }

    /*
     * What the transport measured the link to be worth. Zero until enough has
     * been acknowledged to say anything, and zero if pacing is compiled out.
     */
    {
        uint32_t bw_bps = 0;
        if (tuya_p2p_rtc_get_link_rate(pSession->session, TUYA_VDATA_CHANNEL, &bw_bps, NULL) == 0 && bw_bps > 0) {
            uint32_t bw_kbps     = (uint32_t)(((uint64_t)bw_bps * 8u) / 1000u);
            pSession->rc_bw_kbps = (pSession->rc_bw_kbps == 0)
                                       ? bw_kbps
                                       : ((pSession->rc_bw_kbps * RC_BW_SMOOTH_OLD) + bw_kbps) / RC_BW_SMOOTH_DEN;
        }
    }

    if (pSession->rc_warmup < RC_WARMUP_WINDOWS) {
        pSession->rc_warmup++;
    } else {
        /*
         * Sustained pressure only. A single shed frame is what one oversized
         * key frame looks like on a link that is otherwise keeping up, and
         * treating it as congestion walks the rate down a link that has plenty
         * of room - measured doing so over a LAN.
         */
        BOOL_T overuse = (fills >= RC_FILL_EVENTS_MIN || avg_fill >= RC_FILL_HIGH_PCT) ? TRUE : FALSE;
        BOOL_T drained = (avg_fill <= RC_FILL_LOW_PCT) ? TRUE : FALSE;

        if (pSession->rc_bw_kbps > 0) {
            /*
             * Set the rate from the link, not from the queue.
             *
             * The queue only distinguishes "full" from "empty", and it is full
             * for as long as the transport is still opening its window - which
             * on a slow path takes twenty seconds. Driven by that alone the
             * controller walked 1024 kbps down to its floor while the transport
             * was still ramping, then sat there: measured 289 kbps carried on a
             * link the transport had by then measured at 1540.
             *
             * Occupancy still has a job, but a narrower one. It cannot say how
             * fast the link is; it can say the estimate is currently wrong, so
             * it is kept as the one signal that may override the measurement
             * downwards.
             */
            want = pSession->rc_bw_kbps * RC_BW_SHARE_NUM / RC_BW_SHARE_DEN;
            if (overuse) {
                uint32_t backoff = pSession->rc_cur_kbps * RC_DOWN_NUM / RC_DOWN_DEN;
                if (want > backoff) {
                    want = backoff;
                }
                pSession->rc_state = RC_STATE_DECREASE;
            } else {
                /*
                 * Slew limit. The measurement is a delivery rate, and what gets
                 * delivered depends on what this controller chose to send, so
                 * following it directly is a loop feeding itself: measured the
                 * estimate moving by a factor of fourteen window to window and
                 * the encoder chasing it between 256 and 704 kbps, which reads
                 * on screen as the picture changing sharpness every second.
                 * Congestion above still cuts at once - it is the only signal
                 * here that is not self-inflicted. Everything else moves at the
                 * draft's rate and gets there over a few windows instead.
                 */
                uint32_t ceiling = pSession->rc_cur_kbps * RC_UP_NUM / RC_UP_DEN;
                uint32_t floor_step = pSession->rc_cur_kbps * RC_DOWN_NUM / RC_DOWN_DEN;

                if (want > ceiling) {
                    want = ceiling;
                }
                if (want < floor_step) {
                    want = floor_step;
                }
                pSession->rc_state = drained ? RC_STATE_INCREASE : RC_STATE_HOLD;
            }
        } else {
            /* Nothing measured yet - the queue heuristic is all there is. */
            switch (pSession->rc_state) {
            case RC_STATE_DECREASE:
                /* Keep cutting while it is still congested; once the queue is
                 * back under control, hold rather than immediately climbing. */
                if (overuse) {
                    want = want * RC_DOWN_NUM / RC_DOWN_DEN;
                } else {
                    pSession->rc_state = RC_STATE_HOLD;
                }
                break;

            case RC_STATE_HOLD:
                /* Rate stays put until the backlog has genuinely gone. */
                if (overuse) {
                    want               = want * RC_DOWN_NUM / RC_DOWN_DEN;
                    pSession->rc_state = RC_STATE_DECREASE;
                } else if (drained) {
                    pSession->rc_state = RC_STATE_INCREASE;
                }
                break;

            case RC_STATE_INCREASE:
            default:
                if (overuse) {
                    want               = want * RC_DOWN_NUM / RC_DOWN_DEN;
                    pSession->rc_state = RC_STATE_DECREASE;
                } else if (drained && want < pSession->rc_base_kbps) {
                    want = want * RC_UP_NUM / RC_UP_DEN;
                } else if (!drained) {
                    /* Neither congested nor empty: leave it alone. */
                    pSession->rc_state = RC_STATE_HOLD;
                }
                break;
            }
        }

        if (want < floor_kbps) {
            want = floor_kbps;
        }
        if (want > pSession->rc_base_kbps) {
            want = pSession->rc_base_kbps;
        }
    }

    if (want != pSession->rc_cur_kbps) {
        uint32_t delta =
            (want > pSession->rc_cur_kbps) ? (want - pSession->rc_cur_kbps) : (pSession->rc_cur_kbps - want);

        /* Worth a reconfiguration, or at the floor/ceiling where the exact
         * value matters more than the size of the step. */
        if (delta * 100u >= pSession->rc_cur_kbps * RC_MIN_CHANGE_PCT || want == floor_kbps ||
            want == pSession->rc_base_kbps) {
            /* No literal percent sign: the log formatter renders "%%" as '?'. */
            PR_NOTICE("rate control: %u -> %u kbps [%s] (link %u kbps, queue avg %u pct, peak %u pct, %u full events)",
                      pSession->rc_cur_kbps, want,
                      (pSession->rc_state == RC_STATE_DECREASE)
                          ? "decrease"
                          : ((pSession->rc_state == RC_STATE_HOLD) ? "hold" : "increase"),
                      pSession->rc_bw_kbps, avg_fill, pSession->rc_fill_peak, fills);
            if (pSession->on_set_video_bitrate_callback(want) == 0) {
                pSession->rc_cur_kbps = want;
            }
        }
    }

    pSession->rc_window_ms     = now;
    pSession->rc_full_at_start = pSession->tx_full_cnt;
    pSession->rc_fill_peak     = pSession->tx_fill_pct;
    pSession->rc_fill_sum      = 0;
    pSession->rc_fill_samples  = 0;
}

/*
 * Shortest gap between key frame requests.
 *
 * A key frame is the largest frame the encoder makes, so asking for one while
 * the send queue is already full is the worst possible moment to add work. The
 * congestion path retries every backoff interval, and without this it asked on
 * every attempt: measured on hardware that produced 17 to 26 key frames in a
 * single second against a 2 second GOP, driving output to 2.8 Mbps while the
 * target was 432 kbps. The recovery mechanism was making the congestion it was
 * recovering from. One request, then wait long enough to see whether it
 * arrived and helped.
 */
#define P2P_IFRAME_REQ_MIN_GAP_MS 1500u

/* Backlog considered drained enough to accept a key frame, in percent of the
 * latency budget. */
#define P2P_TX_DRAINED_PCT 40u

/**
 * @brief Ask the source for a key frame, if it can provide one on demand.
 * @return TRUE when the request was accepted
 */
static BOOL_T __p2p_request_i_frame(P2P_SESSION_T *pSession)
{
    uint64_t now;

    if (pSession->on_request_i_frame_callback == NULL) {
        return FALSE;
    }
    now = (uint64_t)tal_system_get_millisecond();
    if (pSession->iframe_req_ms != 0 && (now - pSession->iframe_req_ms) < P2P_IFRAME_REQ_MIN_GAP_MS) {
        return FALSE;
    }
    pSession->iframe_req_ms = now;
    return (pSession->on_request_i_frame_callback() == 0) ? TRUE : FALSE;
}

#define P2P_VIDEO_RTP_CLOCK_HZ 90

/**
 * @brief Map media frame wall-clock ms to RTP 90 kHz timestamp (RFC 6184)
 * @return RTP timestamp in 90 kHz units
 * @note v_pts is stored in microseconds; RTP layer expects 90 kHz, not us.
 */
static uint32_t __p2p_video_rtp_timestamp_ms90(void)
{
    uint64_t ms;

    ms = sg_p2p_session->v_timestamp;
    if (ms == 0 && sg_p2p_session->v_pts != 0) {
        ms = sg_p2p_session->v_pts / 1000ULL;
    }
    return (uint32_t)(ms * (uint64_t)P2P_VIDEO_RTP_CLOCK_HZ);
}

/**
 * @brief Validate Annex-B H264 access unit before RTP pack (avoid assert on device)
 * @param[in] pData frame buffer
 * @param[in] len byte length
 * @return TRUE if at least one NAL with payload length > 0
 */
static BOOL_T __p2p_h264_annexb_au_valid(const char *pData, int len)
{
    const uint8_t *p;
    const uint8_t *end;
    const uint8_t *next;
    int n;
    int i;
    BOOL_T has_nal = FALSE;

    if (pData == NULL || len < 5) {
        return FALSE;
    }
    p = (const uint8_t *)pData;
    end = p + len;
    for (i = 0; i + 3 < len; i++) {
        if (p[i] == 0x00 && p[i + 1] == 0x00 && p[i + 2] == 0x01) {
            p = p + i + 3;
            break;
        }
        if (i + 4 < len && p[i] == 0x00 && p[i + 1] == 0x00 && p[i + 2] == 0x00 && p[i + 3] == 0x01) {
            p = p + i + 4;
            break;
        }
    }
    if (p >= end) {
        return FALSE;
    }
    while (p < end) {
        next = NULL;
        for (i = 0; p + i + 3 < end; i++) {
            if (p[i] == 0x00 && p[i + 1] == 0x00 && p[i + 2] == 0x01) {
                next = p + i;
                break;
            }
            if (p + i + 4 < end && p[i] == 0x00 && p[i + 1] == 0x00 && p[i + 2] == 0x00 && p[i + 3] == 0x01) {
                next = p + i;
                break;
            }
        }
        if (next) {
            n = (int)(next - p);
            if (n >= 3 && p[n - 1] == 0x00 && p[n - 2] == 0x00) {
                n -= 3;
            }
        } else {
            n = (int)(end - p);
        }
        while (n > 0 && p[n - 1] == 0x00) {
            n--;
        }
        if (n > 0) {
            has_nal = TRUE;
        }
        if (next == NULL) {
            break;
        }
        if (next + 3 < end && next[2] == 0x01) {
            p = next + 3;
        } else if (next + 4 < end) {
            p = next + 4;
        } else {
            break;
        }
    }
    return has_nal;
}

/***********************************************************
 *  Function: __p2p_pack_h265_rtp_and_send
 *  Note:IPC stream data assembly RTP and send
 *  Input: pData data header address, len data length, client channel number
 *  Output: none
 *  Return:
 ***********************************************************/
static OPERATE_RET __p2p_pack_h265_rtp_and_send(int client, char *pData, int len)
{
    if (NULL == pData) {
        PR_ERR("input error");
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = __p2p_check_free_buffer_size(client, TUYA_VDATA_CHANNEL, len);
    if (OPRT_OK != ret) {
        return ret;
    }

    if (NULL == sg_p2p_session->p_video_rtp_buff) {
        PR_ERR("video rtp buffer is NULL");
        return OPRT_INVALID_PARM;
    }

    RTP_PACK_NAL_ARG_T rtp_pack_nal_arg;
    rtp_pack_nal_arg.client = client;
    rtp_pack_nal_arg.channel = TUYA_VDATA_CHANNEL;
    rtp_pack_nal_arg.p_rtp_buff = sg_p2p_session->p_video_rtp_buff;
    rtp_pack_nal_arg.sent_pkts  = 0;
    memset(rtp_pack_nal_arg.ext_head_buff, 0, P2P_EXT_HEAD_MAX_LEN);
    __p2p_ext_protocol_pack(client, 0, rtp_pack_nal_arg.ext_head_buff, &rtp_pack_nal_arg.fix_len);

    void *pRtpDelegate = NULL;
    struct rtp_payload_t rtp_packer;
    rtp_packer.alloc = rtp_alloc;
    rtp_packer.free = rtp_free;
    rtp_packer.packet = rtp_pack_packet_handler;
    uint16_t seq = sg_p2p_session->video_seq_num;
    uint32_t ssrc = 10;
    uint32_t timestamp = __p2p_video_rtp_timestamp_ms90();
    /* 95 is Tuya's H265_PAY_LOAD, which the App keys off: nothing else on the
     * wire says which codec this is - MEDIA_FRAME_T carries no codec and the
     * App never queries video params. See rtp_payload_find() for why the
     * library had to be taught this number. */
    pRtpDelegate = rtp_payload_encode_create(/*H265_PAY_LOAD*/ 95, "H265", seq, ssrc, &rtp_packer, &rtp_pack_nal_arg);
    if (NULL == pRtpDelegate) {
        PR_ERR("rtp_payload_encode_create h265 failed");
        return OPRT_COM_ERROR;
    }
    ret                = rtp_payload_encode_input(pRtpDelegate, pData, len, timestamp);
    rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->video_seq_num, &timestamp);
    rtp_payload_encode_destroy(pRtpDelegate);
    if (0 != ret) {
        /* Same rule as H264: a frame that is already partly transmitted must
         * not be offered again. Also map the packer's raw -1 onto an OPRT code
         * so the caller can tell retryable from terminal at all. */
        PR_ERR("rtp_payload_encode_input h265 error:%d after %d packets", ret, rtp_pack_nal_arg.sent_pkts);
        return (rtp_pack_nal_arg.sent_pkts == 0) ? OPRT_RESOURCE_NOT_READY : OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_pack_h264_rtp_and_send
 *  Note:IPC stream data assembly RTP and send
 *  Input: pData data header address, len data length, client channel number
 *  Output: none
 *  Return:
 ***********************************************************/
static OPERATE_RET __p2p_pack_h264_rtp_and_send(int client, char *pData, int len)
{
    if (NULL == pData) {
        PR_ERR("input error");
        return OPRT_INVALID_PARM;
    }

    uint32_t max_frame_size = /*tuya_ipc_media_adapter_get_max_frame(0, 0, 0)*/ (300 * 1024);
    if (len > max_frame_size) {
        PR_ERR("frame len too big[%d]", len);
        return OPRT_INVALID_PARM;
    }
    if (!__p2p_h264_annexb_au_valid(pData, len)) {
        static uint32_t s_h264_bad_cnt = 0;
        if ((s_h264_bad_cnt++ % 30) == 0) {
            PR_WARN("skip invalid H264 AU len=%d cnt=%u", len, (uint32_t)s_h264_bad_cnt);
        }
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret;
    ret = __p2p_check_free_buffer_size(client, TUYA_VDATA_CHANNEL, len);
    if (OPRT_OK != ret) {
        return ret;
    }

    if (NULL == sg_p2p_session->p_video_rtp_buff) {
        PR_ERR("video rtp buffer is NULL");
        return OPRT_INVALID_PARM;
    }

    RTP_PACK_NAL_ARG_T rtp_pack_nal_arg;
    rtp_pack_nal_arg.client = client;
    rtp_pack_nal_arg.channel = TUYA_VDATA_CHANNEL;
    rtp_pack_nal_arg.p_rtp_buff = sg_p2p_session->p_video_rtp_buff;
    rtp_pack_nal_arg.sent_pkts  = 0;
    memset(rtp_pack_nal_arg.ext_head_buff, 0, P2P_EXT_HEAD_MAX_LEN);
    __p2p_ext_protocol_pack(client, 0, rtp_pack_nal_arg.ext_head_buff, &rtp_pack_nal_arg.fix_len);

    void *pRtpDelegate = NULL;
    struct rtp_payload_t rtp_packer;
    rtp_packer.alloc = rtp_alloc;
    rtp_packer.free = rtp_free;
    rtp_packer.packet = rtp_pack_packet_handler;
    uint16_t seq = sg_p2p_session->video_seq_num;
    uint32_t ssrc = 10;
    uint32_t timestamp = __p2p_video_rtp_timestamp_ms90();
    pRtpDelegate = rtp_payload_encode_create(/*H264_PAY_LOAD*/ 96, "H264", seq, ssrc, &rtp_packer, &rtp_pack_nal_arg);
    ret = rtp_payload_encode_input(pRtpDelegate, pData, len, timestamp);
    if (ret != 0) {
        PR_ERR("rtp_payload_encode_input h264 error:%d after %d packets", ret, rtp_pack_nal_arg.sent_pkts);
        rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->video_seq_num, &timestamp);
        rtp_payload_encode_destroy(pRtpDelegate);
        /*
         * Retry only if nothing went out. A frame is emitted as many RTP
         * packets, so once some of them are gone the peer has a fragment it
         * will never complete; re-sending the frame from the start just adds a
         * duplicate copy behind that fragment. Report it as unrecoverable so
         * the caller drops the frame and waits for a clean one.
         */
        return (rtp_pack_nal_arg.sent_pkts == 0) ? OPRT_RESOURCE_NOT_READY : OPRT_COM_ERROR;
    }
    rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->video_seq_num, &timestamp);
    rtp_payload_encode_destroy(pRtpDelegate);

    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_pack_aac_rtp_and_send
 *  Note:IPC stream data assembly RTP and send
 *  Input: pData data header address, len data length, client channel number
 *  Output: none
 *  Return:
 ***********************************************************/
// static OPERATE_RET __p2p_pack_aac_rtp_and_send(int client, char *pData, int len)
// {
//     if (NULL == pData) {
//         PR_ERR("data[%p] client num [%d]",pData, client);
//         return OPRT_INVALID_PARM;
//     }
//     //Process according to 1-n frames
//     int i;
//     OPERATE_RET ret = OPRT_OK;
//     ADTS_HEADER strAdts = {0};
//     int audioRtpLen = 0;

//     PR_DEBUG("aac audio len[%d]",len);

// 	ret = __p2p_check_free_buffer_size(client,TUYA_ADATA_CHANNEL,len);
// 	if (OPRT_OK != ret) {
//         return ret;
//     }

//     if (NULL == sg_p2p_session->p_audio_rtp_buff) {
//         PR_ERR("audio rtp buffer is NULL");
//         return OPRT_INVALID_PARM;
//     }

//     int fix_len = 0; //20180428 Added header data
//     char ext_head_buff[P2P_EXT_HEAD_MAX_LEN] = {0};    //Based on extended video header protocol
//     head+ext(8)+rtp_len

//     __p2p_ext_protocol_pack(client, 1, ext_head_buff, &fix_len);

//     for (i = 0; i < len;) {
//         //ADTS header parsing
//         if (OPRT_OK != tuya_ipc_parse_adts_header((uint8_t * )&pData[i], &strAdts)) {
//             i++;
//             continue;
//         }
//         tuya_ipc_show_adts_info(&strAdts);
//         PR_TRACE("parse aac frame length = %d len[%d]",strAdts.aac_frame_length,len);
//         //Length verification
//         if (i + strAdts.aac_frame_length > len) {
//             PR_ERR("calc len error parse index[%d]aac_len[%d]len[%d]",i, strAdts.aac_frame_length, len);
//             return OPRT_COM_ERROR;
//         }
//         PR_TRACE("parse aac i[%d] data_len[%d]",i,strAdts.aac_frame_length - ADTS_HEADER_LENGTH);
//         if (strAdts.aac_frame_length - ADTS_HEADER_LENGTH < P2P_RTP_PACK_LEN) {
//             if (OPRT_OK == tuya_ipc_pack_aac_rtp((uint8_t * )(pData + i + ADTS_HEADER_LENGTH),
//             strAdts.aac_frame_length - ADTS_HEADER_LENGTH,\
//                 &audioRtpLen, sg_p2p_session->p_audio_rtp_buff + fix_len,client)) {

//                 memcpy(sg_p2p_session->p_audio_rtp_buff, ext_head_buff, fix_len);
//                 *(int *)&sg_p2p_session->p_audio_rtp_buff[fix_len - 4] = audioRtpLen;
//                 audioRtpLen += fix_len;

//                 ret = __p2p_send_rtp_data(client, TUYA_ADATA_CHANNEL,sg_p2p_session->p_audio_rtp_buff,audioRtpLen);
//             }
//         } else {
//             PR_DEBUG("aac data too big [%d] [%d]",P2P_RTP_PACK_LEN,strAdts.aac_frame_length);
//         }
//         i += strAdts.aac_frame_length;
//         PR_DEBUG("parse aac i[%d]",i);
//     }
//     return ret;
// }

/***********************************************************
 *  Function: __p2p_pack_g711_rtp_and_send
 *  Note:IPC audio data assembly RTP and send
 *  Input: pData data header address, len data length, client channel number, mode g711 mode
 *  Output: none
 *  Return:
 ***********************************************************/
static OPERATE_RET __p2p_pack_g711_rtp_and_send(int client, char *pData, int len, int mode)
{
    if (NULL == pData) {
        PR_ERR("data[%p] client num [%d]", pData, client);
        return OPRT_INVALID_PARM;
    }

    if (len > P2P_RTP_PACK_LEN) {
        PR_ERR("data too big %d", len);
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = OPRT_OK;
    ret = __p2p_check_free_buffer_size(client, TUYA_ADATA_CHANNEL, len);
    if (OPRT_OK != ret) {
        return ret;
    }

    if (NULL == sg_p2p_session->p_audio_rtp_buff) {
        PR_ERR("audio rtp buffer is NULL");
        return OPRT_INVALID_PARM;
    }

    RTP_PACK_NAL_ARG_T rtp_pack_nal_arg;
    rtp_pack_nal_arg.client = client;
    rtp_pack_nal_arg.channel = TUYA_ADATA_CHANNEL;
    rtp_pack_nal_arg.p_rtp_buff = sg_p2p_session->p_audio_rtp_buff;
    rtp_pack_nal_arg.sent_pkts  = 0;
    memset(rtp_pack_nal_arg.ext_head_buff, 0, P2P_EXT_HEAD_MAX_LEN);
    __p2p_ext_protocol_pack(client, 1, rtp_pack_nal_arg.ext_head_buff, &rtp_pack_nal_arg.fix_len);

    void *pRtpDelegate = NULL;
    struct rtp_payload_t rtp_packer;
    rtp_packer.alloc = rtp_alloc;
    rtp_packer.free = rtp_free;
    rtp_packer.packet = rtp_pack_packet_handler;
    uint16_t seq = sg_p2p_session->audio_seq_num;
    uint32_t ssrc = 11;
    uint32_t timestamp = (uint32_t)sg_p2p_session->a_pts;
    int payload = 0;
    char *codec_name = NULL;
    if (TY_AV_CODEC_AUDIO_G711U == mode) {
        codec_name = "PCMU";
        payload = 0 /*RTP_PCMU_PAYLOAD*/;
    } else if (TY_AV_CODEC_AUDIO_G711A == mode) {
        codec_name = "PCMA";
        payload = 8 /*RTP_PCMA_PAYLOAD*/;
    } else {
        codec_name = "PCM";
        payload = 99 /*RTP_PCM_PAYLOAD*/;
    }
    pRtpDelegate = rtp_payload_encode_create(payload, codec_name, seq, ssrc, &rtp_packer, &rtp_pack_nal_arg);
    ret          = rtp_payload_encode_input(pRtpDelegate, pData, len, timestamp);
    rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->audio_seq_num, &timestamp);
    rtp_payload_encode_destroy(pRtpDelegate);
    if (0 != ret) {
        PR_ERR("rtp_payload_encode_input %s error:%d after %d packets", codec_name, ret, rtp_pack_nal_arg.sent_pkts);
        return (rtp_pack_nal_arg.sent_pkts == 0) ? OPRT_RESOURCE_NOT_READY : OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

OPERATE_RET tuya_ipc_delete_video_finish_v2(const uint32_t client, TUYA_DOWNLOAD_DATA_TYPE type, int success)
{
    return OPRT_OK;
}

OPERATE_RET tuya_ipc_p2p_set_limit_mode(BOOL_T islimit)
{
    return OPRT_OK;
}

/**
 * @brief Sync speaker decode params from encode av_Info (align OS audio_decode_info default)
 * @param[in] av_info encode/stream AV info
 * @return none
 * @note OS QUERY_AUDIO_PARAMS returns decode_info, not uplink encode. Until product sets a
 *       dedicated decode codec, mirror encode (G711U) so App knows what to push on ADATA.
 */
static void __p2p_sync_speak_audio_from_av(const TRANS_IPC_AV_INFO_T *av_info)
{
    if (NULL == sg_p2p_session || NULL == av_info) {
        return;
    }
    sg_p2p_session->speak_audio_enable = TRUE;
    sg_p2p_session->speak_audio_codec = av_info->audio_codec;
    sg_p2p_session->speak_audio_sample = av_info->audio_sample;
    sg_p2p_session->speak_audio_databits = av_info->audio_databits;
    sg_p2p_session->speak_audio_channel = av_info->audio_channel;
}

OPERATE_RET tuya_ipc_init_trans_av_info(TRANS_IPC_AV_INFO_T *av_info)
{
    if (NULL == sg_p2p_session || NULL == av_info) {
        PR_ERR("tuya_ipc_init_trans_av_info invalid parm");
        return OPRT_INVALID_PARM;
    }
    memcpy(&sg_p2p_session->av_Info, av_info, sizeof(TRANS_IPC_AV_INFO_T));
    __p2p_sync_speak_audio_from_av(av_info);
    PR_DEBUG("main %ux%u@%u codec=0x%x sub %ux%u@%u codec=0x%x", sg_p2p_session->av_Info.width[eIpcStreamVideoMain], sg_p2p_session->av_Info.height[eIpcStreamVideoMain],
              sg_p2p_session->av_Info.fps[eIpcStreamVideoMain],
              (uint32_t)sg_p2p_session->av_Info.video_codec[eIpcStreamVideoMain],
              sg_p2p_session->av_Info.width[eIpcStreamVideoSub], sg_p2p_session->av_Info.height[eIpcStreamVideoSub],
              sg_p2p_session->av_Info.fps[eIpcStreamVideoSub],
              (uint32_t)sg_p2p_session->av_Info.video_codec[eIpcStreamVideoSub]);
    PR_DEBUG("enable=%u codec=0x%x sample=%u bits=%u chn=%u", (uint32_t)sg_p2p_session->speak_audio_enable, (uint32_t)sg_p2p_session->speak_audio_codec,
              (uint32_t)sg_p2p_session->speak_audio_sample, (uint32_t)sg_p2p_session->speak_audio_databits,
              (uint32_t)sg_p2p_session->speak_audio_channel);
    return OPRT_OK;
}

OPERATE_RET tuya_p2p_rtc_register_get_video_frame_cb(tuya_p2p_rtc_get_frame_cb_t pCallback)
{
    sg_p2p_session->on_get_video_frame_callback = pCallback;
    return OPRT_OK;
}

OPERATE_RET tuya_p2p_rtc_register_get_audio_frame_cb(tuya_p2p_rtc_get_frame_cb_t pCallback)
{
    sg_p2p_session->on_get_audio_frame_callback = pCallback;
    return OPRT_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

/***********************************************************
 *  Function: __p2p_session_trans_start
 *  Note:Start p2p transmission, request transmission resources
 *  Input:pSession session management interface
 *  Output: none
 *  Return:
 ***********************************************************/
static int __p2p_session_trans_video_start(P2P_SESSION_T *pSession)
{
    if (NULL == pSession) {
        PR_ERR("video start: no session");
        return OPRT_INVALID_PARM;
    }
    if (P2P_VIDEO & pSession->cmd) {
        /* The App repeats START when its decoder has no key frame to lock onto.
         * Rejecting it is why tapping retry never helps: send one instead. */
        (void)__p2p_request_i_frame(pSession);
        PR_DEBUG("session[%d] video start repeated, key frame requested", pSession->session);
        return OPRT_OK;
    }
    // Wait for previous data transmission to end
    PR_DEBUG("session[%d]video video_start wait_concurr_idle", pSession->session);
    if (pSession->cmd & (P2P_PB_VIDEO | P2P_PB_PAUSE | P2P_PB_AUDIO)) {
        uint32_t vdrop = 0;
        uint32_t adrop = 0;

        PR_NOTICE("live start: clear playback cmd=0x%x", (unsigned)pSession->cmd);
        pSession->cmd = (P2P_CMD_E)(pSession->cmd & ~(P2P_PB_VIDEO | P2P_PB_PAUSE | P2P_PB_AUDIO));
        (void)tuya_p2p_rtc_drop_unsent(pSession->session, TUYA_VDATA_CHANNEL, &vdrop);
        (void)tuya_p2p_rtc_drop_unsent(pSession->session, TUYA_ADATA_CHANNEL, &adrop);
        if (vdrop != 0 || adrop != 0) {
            PR_NOTICE("live start: drop leftover pb v=%u a=%u", vdrop, adrop);
        }
    }
    pSession->cmd |= P2P_VIDEO;
    pSession->video_need_iframe = TRUE;
    pSession->key_frame = FALSE;
    pSession->dbg_vsend_ok = 0;
    pSession->dbg_vsend_skip = 0;
    pSession->dbg_vget_fail = 0;
    pSession->video_frame_pending = FALSE;

    /* Rate control starts from whatever the stream is configured to produce,
     * which is also its ceiling, so pick that up here. */
    pSession->rc_base_kbps = pSession->av_Info.bitrate[p2p_get_chn_idx(pSession->cur_clarity)];
    pSession->rc_cur_kbps  = pSession->rc_base_kbps;
    /* A new session gets a new transport and a new estimate; carrying the old
     * link's capacity across would set the opening rate from a path that is no
     * longer in use. */
    pSession->rc_bw_kbps       = 0;
    pSession->rc_window_ms     = 0;
    pSession->rc_fill_peak     = 0;
    pSession->rc_fill_sum      = 0;
    pSession->rc_fill_samples  = 0;
    pSession->rc_warmup        = 0;
    pSession->rc_state         = RC_STATE_INCREASE;
    pSession->tx_max_frame     = 0;
    pSession->rc_full_at_start = pSession->tx_full_cnt;
    pSession->iframe_req_ms    = 0;

    /* Stop PB before bringing the camera up, so both do not sit on VDATA. */
    (void)tuya_ipc_media_stream_event_call(0, 0, MEDIA_STREAM_LIVE_VIDEO_START, NULL);
    if (pSession->on_live_video_start_callback) {
        (void)pSession->on_live_video_start_callback();
    }

    /*
     * Nothing can be decoded before a key frame arrives, so ask for one now.
     * Waiting for the next scheduled one costs up to a full GOP of black
     * screen at the very moment the user is looking at the app.
     */
    if (__p2p_request_i_frame(pSession)) {
        PR_DEBUG("session[%d] video start, key frame requested", pSession->session);
    } else {
        PR_DEBUG("session[%d] video start success (wait first I-frame)", pSession->session);
    }
    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_session_trans_stop
 *  Note:Close transmission
 *  Input:pSession Session management
 *  Output: none
 *  Return:
 ***********************************************************/
static int __p2p_session_trans_video_stop(P2P_SESSION_T *pSession)
{
    if (NULL == pSession) {
        PR_ERR("video stop: no session");
        return OPRT_INVALID_PARM;
    }
    if (!(P2P_VIDEO & pSession->cmd)) {
        /* The App repeats STOP; video is already down, so this is not a fault. */
        PR_DEBUG("video stop ignored, already stopped cmd[%d]", pSession->cmd);
        return OPRT_OK;
    }
    pSession->cmd &= ~P2P_VIDEO;
    pSession->video_frame_pending = FALSE;
    if (pSession->on_live_video_stop_callback) {
        (void)pSession->on_live_video_stop_callback();
    }
    (void)tuya_ipc_media_stream_event_call(0, 0, MEDIA_STREAM_LIVE_VIDEO_STOP, NULL);
    /* Drop unread LIVE backlog so calendar PB can use VDATA budget */
    (void)tuya_p2p_rtc_clear_send_buffer(pSession->session, TUYA_VDATA_CHANNEL);
    PR_DEBUG("session[%d] video stop success", pSession->session);
    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_session_trans_audio_start
 *  Note:Start p2p audio transmission, apply for transmission resources
 *  Input:pSession session management interface
 *  Output: none
 *  Return:
 ***********************************************************/
static int __p2p_session_trans_audio_start(P2P_SESSION_T *pSession)
{
    if (NULL == pSession || (P2P_AUDIO & pSession->cmd)) {
        PR_ERR("param error or audio started");
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("session[%d] send audio start to dev", pSession->session);
    pSession->cmd |= P2P_AUDIO;
    PR_DEBUG("session:[%d] audio start success", pSession->session);
    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_session_trans_audio_stop
 *  Note:Close transmission
 *  Input:pSession Session management
 *  Output: none
 *  Return:
 ***********************************************************/
static int __p2p_session_trans_audio_stop(P2P_SESSION_T *pSession)
{
    if (NULL == pSession) {
        PR_ERR("audio stop: no session");
        return OPRT_INVALID_PARM;
    }
    if (!(P2P_AUDIO & pSession->cmd)) {
        /* Same repeat-STOP path as video: already stopped is the wanted state. */
        PR_DEBUG("audio stop ignored, already stopped");
        return OPRT_OK;
    }
    pSession->cmd &= ~P2P_AUDIO;
    (void)tuya_p2p_rtc_clear_send_buffer(pSession->session, TUYA_ADATA_CHANNEL);
    PR_DEBUG("session:[%d] audio stop success", pSession->session);
    return OPRT_OK;
}

/**
 * @brief Parse ADATA downlink into audio payload (align OS __p2p_recv_media / SPEAKER_FRAME)
 * @param[in] buf recv buffer
 * @param[in] len recv length
 * @param[out] out_data payload pointer inside buf
 * @param[out] out_size payload size
 * @param[out] kind 0=custom SPEAKER head, 1=std RTP, 2=raw
 * @return OPRT_OK on success
 */
static OPERATE_RET __p2p_parse_downlink_audio(char *buf, int len, void **out_data, uint32_t *out_size,
                                              uint32_t *kind)
{
    struct rtp_packet_t pkt;

    if (NULL == buf || len <= 0 || NULL == out_data || NULL == out_size || NULL == kind) {
        return OPRT_INVALID_PARM;
    }
    *out_data = NULL;
    *out_size = 0;
    *kind = 2;

    /* Align OS: custom frame magic TUYA_RTP_HEAD(0x12345678) + 32B header + payload */
    if (len >= 32) {
        uint32_t magic = 0;
        uint32_t payload_len = 0;

        memcpy(&magic, buf, sizeof(magic));
        if (magic == (uint32_t)TUYA_RTP_HEAD) {
            memcpy(&payload_len, buf + 28, sizeof(payload_len));
            if (payload_len > 0 && payload_len <= (uint32_t)(len - 32) && payload_len <= 1500) {
                *out_data = (void *)(buf + 32);
                *out_size = payload_len;
                *kind = 0;
                return OPRT_OK;
            }
            /* Header present but length odd: take remaining bytes after 32B */
            if (len > 32) {
                *out_data = (void *)(buf + 32);
                *out_size = (uint32_t)(len - 32);
                *kind = 0;
                return OPRT_OK;
            }
        }
    }

    if (rtp_packet_deserialize(&pkt, buf, len) == 0 && pkt.payloadlen > 0) {
        *out_data = (void *)pkt.payload;
        *out_size = (uint32_t)pkt.payloadlen;
        *kind = 1;
        return OPRT_OK;
    }

    *out_data = (void *)buf;
    *out_size = (uint32_t)len;
    *kind = 2;
    return OPRT_OK;
}

static BOOL_T __p2p_audio_downlink_session_dead(int32_t ret)
{
    return (ret == ERROR_P2P_INVALID_SESSION_HANDLE || ret == ERROR_P2P_SESSION_CLOSED_REMOTE ||
            ret == ERROR_P2P_SESSION_CLOSED_TIMEOUT || ret == ERROR_P2P_SESSION_CLOSED_CALLED ||
            ret == ERROR_P2P_NOT_INITIALIZED)
               ? TRUE
               : FALSE;
}

/**
 * @brief Stop downlink recv + app speaker. Safe to call twice.
 * @note Must not run on p2p_audio_dl itself (it deletes that thread).
 */
static void __p2p_audio_downlink_stop(P2P_SESSION_T *pSession, int channel)
{
    THREAD_HANDLE h = NULL;
    BOOL_T        was_on = FALSE;

    if (pSession == NULL) {
        return;
    }

    tal_mutex_lock(pSession->cmutex);
    was_on = (pSession->audio_downlink_on || pSession->audio_downlink_thread != NULL) ? TRUE : FALSE;
    pSession->audio_downlink_on = FALSE;
    pSession->cmd = (P2P_CMD_E)(pSession->cmd & ~P2P_SPEAKER);
    pSession->speak_req_id = -1;
    h = pSession->audio_downlink_thread;
    pSession->audio_downlink_thread = NULL;
    tal_mutex_unlock(pSession->cmutex);

    if (h != NULL) {
        tal_thread_delete(h);
    }
    if (!was_on) {
        return;
    }
    PR_NOTICE("p2p audio downlink stop");
    if (pSession->on_live_audio_stop_callback) {
        (void)pSession->on_live_audio_stop_callback();
    }
    (void)tuya_ipc_media_stream_event_call(0, channel, MEDIA_STREAM_SPEAKER_STOP, NULL);
}

/**
 * @brief Downlink audio recv thread: APP -> device speaker (align TuyaOS on_recv_audio)
 * @note Recv SPEAKER_FRAME / RTP on ADATA, parse payload, callback app to decode+play.
 *       ADATA KCP channel is full-duplex: device sends uplink + recvs downlink here.
 */
static void __p2p_audio_downlink_recv_proc(void *pArg)
{
    char buf[1500];
    uint32_t ok_cnt = 0;
    uint32_t raw_cnt = 0;
    uint32_t speak_cnt = 0;
    uint32_t fail_cnt = 0;
    uint32_t empty_cnt = 0;

    (void)pArg;
    PR_DEBUG("audio recv thread start");
    while (sg_p2p_session && sg_p2p_session->audio_downlink_on) {
        int32_t len = (int32_t)sizeof(buf);
        int32_t ret = tuya_p2p_rtc_recv_data(sg_p2p_session->session, TUYA_ADATA_CHANNEL, buf, &len, 100);
        if (ret == 0 && len > 0) {
            MEDIA_FRAME mf;
            void *payload = NULL;
            uint32_t payload_len = 0;
            uint32_t kind = 2;

            memset(&mf, 0, sizeof(mf));
            mf.type = eAudioFrame;
            if (OPRT_OK == __p2p_parse_downlink_audio(buf, len, &payload, &payload_len, &kind) &&
                payload_len > 0 && payload != NULL) {
                mf.data = payload;
                mf.size = payload_len;
                if (kind == 0) {
                    speak_cnt++;
                } else if (kind == 2) {
                    raw_cnt++;
                }
            }
            ok_cnt++;
            if (mf.size > 0 && sg_p2p_session->on_recv_audio_frame_callback) {
                sg_p2p_session->on_recv_audio_frame_callback(&mf);
            }
        } else if (ret == 0) {
            empty_cnt++;
        } else if (__p2p_audio_downlink_session_dead(ret)) {
            PR_ERR("__p2p_rtc_recv_data failed [%d], stop downlink", ret);
            break;
        } else {
            fail_cnt++;
            if (fail_cnt <= 3 || (fail_cnt % 50) == 1) {
                PR_ERR("__p2p_rtc_recv_data failed [%d]", ret);
            }
            tal_system_sleep(5);
        }
    }
    PR_DEBUG("session recv audio cnt [%d]", (int)ok_cnt);
}

/***********************************************************
 *  Function: __p2p_session_pack_resp
 *  Note:Response to app query
 *  Input:pSrc  Received data, pPayLoad Queried payload data
 *  Output: none
 *  Return:
 ***********************************************************/
static int __p2p_session_pack_resp(P2P_SESSION_T *pSession, void *pSrc, void *pPayLoad, int len)
{
    char *sendBuff = NULL;
    int packLen = 0;
    int ret = 0;

    if (NULL == pSrc || NULL == pPayLoad || NULL == pSession) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    packLen = P2P_CMD_HEAD_LEN + len;
    sendBuff = (char *)Malloc(packLen);
    if (NULL == sendBuff) {
        PR_ERR("malloc failed len[%d]", len);
        return OPRT_MALLOC_FAILED;
    }

    memcpy(sendBuff, pSrc, P2P_CMD_HEAD_LEN);
    ((P2P_CMD_PARSE_T *)sendBuff)->str_header.type = 1;
    ((P2P_CMD_PARSE_T *)sendBuff)->str_header.length = len;
    memcpy(sendBuff + P2P_CMD_HEAD_LEN, pPayLoad, len);

    // Send data
    //    PR_DEBUG("p2p Write data session[%d] chn[%d] len[%d]",pSession->session, TUYA_CMD_CHANNEL, packLen);
    ret = tuya_p2p_rtc_send_data(pSession->session, TUYA_CMD_CHANNEL, sendBuff, packLen, -1);
    if (ret < 0) {
        PR_ERR("p2p Write failed ret = %d", ret);
    }
    Free(sendBuff);
    sendBuff = NULL;
    return ret;
}

static int __p2p_session_cmd_parse_server(P2P_SESSION_T *pSession, void *pData)
{
    P2P_CMD_PARSE_T *pCmd = NULL;
    C2C_CMD_FIXED_HEADER_T *pFixedHead = NULL;
    char *pPayload = NULL;

    if (NULL == pSession || NULL == pData) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    pPayload = pData + P2P_CMD_HEAD_LEN;
    pCmd = (P2P_CMD_PARSE_T *)pData;
    pFixedHead = &pCmd->str_header;

    switch (pFixedHead->high_cmd) {
    case TY_C2C_CMD_QUERY_FIXED_ABILITY:
    case TY_C2C_CMD_QUERY_FIXED_ABILITY_GW: {
        C2C_TRANS_QUERY_FIXED_ABI_REQ *pAbiReq = (C2C_TRANS_QUERY_FIXED_ABI_REQ *)pPayload;
        C2C_TRANS_QUERY_FIXED_ABI_RESP abiResp;

        memset(&abiResp, 0, sizeof(abiResp));
        abiResp.channel = pAbiReq->channel;
        /* Align TuyaOS: report video + mic + speaker so App enables uplink/downlink */
        abiResp.ability_mask = (TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_VIDEO |
                                TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_SPEAKER |
                                TY_CMD_QUERY_IPC_FIXED_ABILITY_TYPE_MIC);
        PR_DEBUG("ch=%u mask=0x%x (cmd=%u)", abiResp.channel, abiResp.ability_mask,
                  (uint32_t)pFixedHead->high_cmd);
        __p2p_session_pack_resp(pSession, pData, &abiResp, sizeof(abiResp));
        break;
    }
    case TY_C2C_CMD_QUERY_AUDIO_PARAMS: {
        // Query audio parameters — App uses this for intercom encode-to-device (align OS decode_info)
        C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E *pAudioResp = NULL;
        C2C_TRANS_QUERY_AUDIO_PARAM_REQ_T *pAudioReq;
        pAudioReq = (C2C_TRANS_QUERY_AUDIO_PARAM_REQ_T *)pPayload;
        int respLen = sizeof(C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E) + sizeof(AUDIO_PARAM_T);
        pAudioResp = (C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E *)Malloc(respLen);
        if (NULL != pAudioResp) {
            memset(pAudioResp, 0, (size_t)respLen);
            pAudioResp->channel = pAudioReq->channel;
            pAudioResp->count = 1;
            /* OS returns p2p_ctl speak_* (from audio_decode_info), not uplink encode alone */
            if (sg_p2p_session->speak_audio_enable) {
                pAudioResp->audioParams[0].type = (unsigned int)sg_p2p_session->speak_audio_codec;
                pAudioResp->audioParams[0].sample_rate = (unsigned int)sg_p2p_session->speak_audio_sample;
                pAudioResp->audioParams[0].bitwidth = (unsigned int)sg_p2p_session->speak_audio_databits;
                pAudioResp->audioParams[0].channel_num = (unsigned int)sg_p2p_session->speak_audio_channel;
            } else {
                pAudioResp->audioParams[0].type = (unsigned int)sg_p2p_session->av_Info.audio_codec;
                pAudioResp->audioParams[0].sample_rate = (unsigned int)sg_p2p_session->av_Info.audio_sample;
                pAudioResp->audioParams[0].bitwidth = (unsigned int)sg_p2p_session->av_Info.audio_databits;
                pAudioResp->audioParams[0].channel_num = (unsigned int)sg_p2p_session->av_Info.audio_channel;
            }
            PR_DEBUG("ch=%u codec=0x%x sample=%u bits=%u chn=%u (speak=%u)", pAudioResp->channel,
                      pAudioResp->audioParams[0].type, pAudioResp->audioParams[0].sample_rate,
                      pAudioResp->audioParams[0].bitwidth, pAudioResp->audioParams[0].channel_num,
                      (uint32_t)sg_p2p_session->speak_audio_enable);
            __p2p_session_pack_resp(pSession, pData, pAudioResp, respLen);
            Free(pAudioResp);
        } else {
            // Send failure message to app
            C2C_CMD_IO_CTRL_COM_RESP_T comResp;
            memset(&comResp, 0x00, sizeof(comResp));
            comResp.channel = pAudioReq->channel;
            comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_FAILED;
            __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(C2C_CMD_IO_CTRL_COM_RESP_T));
        }
        break;
    }
    case TY_C2C_CMD_QUERY_VIDEO_STREAM_PARAMS: {
        // Query video parameters — reply with configured av_Info streams
        C2C_TRANS_QUERY_VIDEO_PARAM_RESP_T *pVideoResp = NULL;
        C2C_TRANS_QUERY_VIDEO_PARAM_REQ_T *pVideoReq;
        uint32_t stream_idx[2];
        uint32_t count = 0;
        int respLen;
        uint32_t i;

        pVideoReq = (C2C_TRANS_QUERY_VIDEO_PARAM_REQ_T *)pPayload;
        /* Order: standard(sub) then high(main), matching clarity enum */
        if (sg_p2p_session->av_Info.width[eIpcStreamVideoSub] > 0) {
            stream_idx[count++] = eIpcStreamVideoSub;
        }
        if (sg_p2p_session->av_Info.width[eIpcStreamVideoMain] > 0) {
            stream_idx[count++] = eIpcStreamVideoMain;
        }
        if (0 == count && sg_p2p_session->av_Info.width[0] > 0) {
            stream_idx[count++] = 0;
        }

        if (count > 0) {
            respLen = (int)(sizeof(C2C_TRANS_QUERY_VIDEO_PARAM_RESP_T) + count * sizeof(VIDEO_PARAM_T));
            pVideoResp = (C2C_TRANS_QUERY_VIDEO_PARAM_RESP_T *)Malloc(respLen);
        }
        if (NULL != pVideoResp) {
            memset(pVideoResp, 0, (size_t)respLen);
            pVideoResp->channel = pVideoReq->channel;
            pVideoResp->count = count;
            for (i = 0; i < count; i++) {
                uint32_t ch = stream_idx[i];
                pVideoResp->VideoParams[i].codec_type = (unsigned int)sg_p2p_session->av_Info.video_codec[ch];
                pVideoResp->VideoParams[i].width = sg_p2p_session->av_Info.width[ch];
                pVideoResp->VideoParams[i].height = sg_p2p_session->av_Info.height[ch];
                pVideoResp->VideoParams[i].frame_rate = sg_p2p_session->av_Info.fps[ch];
            }
            PR_DEBUG("ch=%u count=%u first=%ux%u@%u codec=0x%x", pVideoResp->channel,
                      pVideoResp->count, pVideoResp->VideoParams[0].width, pVideoResp->VideoParams[0].height,
                      pVideoResp->VideoParams[0].frame_rate, pVideoResp->VideoParams[0].codec_type);
            __p2p_session_pack_resp(pSession, pData, pVideoResp, respLen);
            Free(pVideoResp);
        } else {
            // Send failure message to app
            C2C_CMD_IO_CTRL_COM_RESP_T comResp;
            memset(&comResp, 0x00, sizeof(comResp));
            comResp.channel = pVideoReq->channel;
            comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_FAILED;
            PR_DEBUG("fail av_info empty or malloc");
            __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(C2C_CMD_IO_CTRL_COM_RESP_T));
        }
        break;
    }
    case TY_C2C_CMD_QUERY_VIDEO_CLARITY: {
        // Video clarity query
        // Video clarity feedback
        PR_DEBUG("recv session[%d] query video clarity", pSession->session);
        C2C_TRANS_QUERY_VIDEO_CLARITY_RESP_T ClarityResp = {0};
        C2C_TRANS_QUERY_VIDEO_CLARITY_REQ_T *clarityReq;
        clarityReq = (C2C_TRANS_QUERY_VIDEO_CLARITY_REQ_T *)pPayload;

        ClarityResp.channel = clarityReq->channel;
        ClarityResp.sp_mode =
            TY_VIDEO_CLARITY_INNER_STANDARD | TY_VIDEO_CLARITY_INNER_HIGH; // Currently SDK supports fixed format
        // p2p_get_clarity(&ClarityResp.sp_mode);
        PR_DEBUG("get support clarity[%u]", ClarityResp.sp_mode);
        ClarityResp.cur_mode = pSession->cur_clarity;
        __p2p_session_pack_resp(pSession, pData, &ClarityResp, sizeof(C2C_TRANS_QUERY_VIDEO_CLARITY_RESP_T));
        break;
    }
    case TY_C2C_CMD_IO_CTRL_VIDEO: {
        C2C_TRANS_CTRL_VIDEO_REQ_T *parm = (C2C_TRANS_CTRL_VIDEO_REQ_T *)pPayload;
        PR_DEBUG("CTRL VIDEO session[%d] chn[%d] op[%d]", pSession->session, parm->channel, parm->operation);
        C2C_CMD_IO_CTRL_COM_RESP_T comResp;
        memset(&comResp, 0x00, sizeof(comResp));
        comResp.channel = parm->channel;
        comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_RECV;
        __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(C2C_CMD_IO_CTRL_COM_RESP_T));
        switch (parm->operation) {
        case TY_CMD_IO_CTRL_VIDEO_PLAY: {
            // When requesting video, save reqId for response
            pSession->video_req_id = pCmd->reqId;
            //   PR_DEBUG("CTRL VIDEO START session[%d] chn[%d]
            //   op[%d]",pSession->session,parm->channel,parm->operation);
            // 20190416add
            if (0 != parm->channel) {
                PR_DEBUG("session [%d] recv chn[%d]", pSession->session, parm->channel);
                pSession->cur_clarity = parm->channel;
            }
            if (OPRT_OK != __p2p_session_trans_video_start(pSession)) {
                PR_ERR("CTRL VIDEO START failed");
            }
            break;
        }
        case TY_CMD_IO_CTRL_VIDEO_STOP: {
            //   PR_DEBUG("CTRL VIDEO STOP session[%d] chn[%d] op[%d]",pSession->session,parm->channel,parm->operation);
            if (OPRT_OK != __p2p_session_trans_video_stop(pSession)) {
                PR_ERR("CTRL VIDEO STOP failed");
            }
            break;
        }
        case TY_CMD_IO_CTRL_AUDIO_MIC_START: {
            //   PR_DEBUG("CTRL AUDIO START session[%d]",pSession->session);
            pSession->audio_req_id = pCmd->reqId;
            __p2p_session_trans_audio_start(pSession);
            break;
        }
        case TY_CMD_IO_CTRL_AUDIO_MIC_STOP: {
            //   PR_DEBUG("CTRL AUDIO STOP session[%d]",pSession->session);
            __p2p_session_trans_audio_stop(pSession);
            break;
        }
        default:
            PR_ERR("CTRL ERROR chn[%d] op[%d]", parm->channel, parm->operation);
            break;
        }
        break;
    }
    case TY_C2C_CMD_IO_CTRL_VIDEO_CLARITY: {
        C2C_TRANS_CTRL_VIDEO_CLARITY_T *parm = (C2C_TRANS_CTRL_VIDEO_CLARITY_T *)pPayload;
        C2C_TRANS_LIVE_CLARITY_PARAM_S outParm = {0};
        outParm.clarity =
            (parm->mode == TY_VIDEO_CLARITY_INNER_HIGH) ? TY_VIDEO_CLARITY_HIGH : TY_VIDEO_CLARITY_STANDARD;
        /* Keep session clarity in INNER enum used by p2p_get_chn_idx / ext header */
        if (parm->mode == TY_VIDEO_CLARITY_INNER_STANDARD || parm->mode == TY_VIDEO_CLARITY_INNER_HIGH ||
            parm->mode == TY_VIDEO_CLARITY_INNER_PROFLOW || parm->mode == TY_VIDEO_CLARITY_S_INNER_HIGH ||
            parm->mode == TY_VIDEO_CLARITY_SS_INNER_HIGH) {
            pSession->cur_clarity = (TRANSFER_VIDEO_CLARITY_TYPE_INNER_E)parm->mode;
        } else {
            pSession->cur_clarity = TY_VIDEO_CLARITY_INNER_HIGH;
        }
        PR_DEBUG("set video clarity session[%d]chn[%d] op[%d] clarity[%d] cur_inner=%u", pSession->session,
                 parm->channel, parm->mode, outParm.clarity, (uint32_t)pSession->cur_clarity);
        C2C_CMD_IO_CTRL_COM_RESP_T comResp;
        memset(&comResp, 0x00, sizeof(comResp));
        comResp.channel = parm->channel;
        comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_SUCCESS;
        __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(C2C_CMD_IO_CTRL_COM_RESP_T));
        break;
    }
    case TY_C2C_CMD_PROTOCOL_VERSION: {
        // C2C_CMD_PROTOCOL_VERSION_T *parm = (C2C_CMD_PROTOCOL_VERSION_T *)pPayload;
        //  PR_DEBUG("session[%d] recv pro_ver[%d][%d]",pSession->session,parm->version >> 16,parm->version&0xff);
        // Version verification processing to be improved later
        C2C_CMD_PROTOCOL_VERSION_T proVerRsp = {0};
        proVerRsp.version = (C2C_MAJOR_VERSION << 16) | C2C_MINOR_VERSION;
        __p2p_session_pack_resp(pSession, pData, &proVerRsp, sizeof(C2C_CMD_PROTOCOL_VERSION_T));
        break;
    }
    case TY_C2C_CMD_CAPABILITY_EXCHANGE: {
        /*
         * App negotiates optional codecs. Opus != P2P speaker codec here:
         * OS wukong P2P on_recv_audio uses G711U only; Opus is for AI voice player.
         * We still reply 1/1 for App handshake compatibility; QUERY_AUDIO_PARAMS = G711U.
         */
        static const char s_cap_exch_resp[] =
            "{\"cmd\":\"capability_exchange_resp\",\"protocol_version\":1,"
            "\"data\":{\"capabilities\":{\"opus_encode\":1,\"opus_decode\":1}}}";
        int cap_len = (int)strlen(s_cap_exch_resp);
        PR_DEBUG("capability_exchange_resp len=%d", cap_len);
        __p2p_session_pack_resp(pSession, pData, (void *)s_cap_exch_resp, cap_len);
        break;
    }
    case TY_C2C_CMD_IO_CTRL_AUDIO: {
        /* Downlink intercom: APP starts/stops speaker (align TuyaOS MEDIA_STREAM_SPEAKER) */
        C2C_TRANS_CTRL_AUDIO_REQ_T *parm = (C2C_TRANS_CTRL_AUDIO_REQ_T *)pPayload;
        C2C_CMD_IO_CTRL_COM_RESP_T comResp;
        PR_DEBUG("SPEAKER op=%u ch=%u reqId=%d", (uint32_t)parm->operation,
                  (uint32_t)parm->channel, pCmd->reqId);
        memset(&comResp, 0x00, sizeof(comResp));
        comResp.channel = parm->channel;
        /* Align OS: SPEAKER start/stop ACK with COMMAND_RECV(1), not SUCCESS(3) */
        comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_RECV;
        __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(C2C_CMD_IO_CTRL_COM_RESP_T));

        if (parm->operation == TY_CMD_IO_CTRL_AUDIO_SPEAKER_START) {
            pSession->speak_req_id = pCmd->reqId;
            pSession->cmd = (P2P_CMD_E)(pSession->cmd | P2P_SPEAKER);
            if (!pSession->audio_downlink_on) {
                pSession->audio_downlink_on = TRUE;
                if (pSession->on_live_audio_start_callback) {
                    (void) pSession->on_live_audio_start_callback();
                }
                (void)tuya_ipc_media_stream_event_call(0, (int)parm->channel, MEDIA_STREAM_SPEAKER_START, NULL);
                THREAD_CFG_T thrd_param = {0};
                thrd_param.stackDepth = 8192;
                thrd_param.priority = THREAD_PRIO_3;
                thrd_param.thrdname = "p2p_audio_dl";
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
                thrd_param.psram_mode = 1;
#endif
                tal_thread_create_and_start(&pSession->audio_downlink_thread, NULL, NULL,
                                            __p2p_audio_downlink_recv_proc, NULL, &thrd_param);
            }
        } else if (parm->operation == TY_CMD_IO_CTRL_AUDIO_SPEAKER_STOP) {
            __p2p_audio_downlink_stop(pSession, (int)parm->channel);
        } else {
            PR_DEBUG("unknown speaker op=%u", (uint32_t)parm->operation);
        }
        break;
    }
    case TY_C2C_CMD_QUERY_PLAYBACK_INFO:
    case TY_C2C_CMD_QUERY_PLAYBACK_INFO_GW: {
        /* low_cmd: legacy TRANS_* (8/9) or MEDIA_STREAM_* (38/39); also accept 0/1 */
        uint32_t low = (uint32_t)pFixedHead->low_cmd;
        PR_DEBUG("high=%u low=%u len=%u", (uint32_t)pFixedHead->high_cmd, low,
                  (uint32_t)pFixedHead->length);
        if (low == 0 || low == 8 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_QUERY_MONTH_SIMPLIFY) {
            C2C_TRANS_QUERY_PB_MONTH_RESP month_resp;
            memset(&month_resp, 0, sizeof(month_resp));
            if (pFixedHead->length >= 12) {
                memcpy(&month_resp, pPayload, (pFixedHead->length < sizeof(month_resp)) ? pFixedHead->length
                                                                                        : sizeof(month_resp));
            }
            (void)tuya_ipc_media_stream_event_call(0, (int)month_resp.channel,
                                                   MEDIA_STREAM_PLAYBACK_QUERY_MONTH_SIMPLIFY, &month_resp);
            __p2p_session_pack_resp(pSession, pData, &month_resp, sizeof(month_resp));
        } else if (low == 1 || low == 9 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS ||
                   low == (uint32_t)MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS_WITH_ENCRYPT) {
            C2C_TRANS_QUERY_PB_DAY_RESP day_resp;
            uint32_t copy_n;
            memset(&day_resp, 0, sizeof(day_resp));
            copy_n = (pFixedHead->length < 16) ? (uint32_t)pFixedHead->length : 16;
            if (copy_n > 0) {
                memcpy(&day_resp, pPayload, copy_n);
            }
            (void)tuya_ipc_media_stream_event_call(0, (int)day_resp.channel, MEDIA_STREAM_PLAYBACK_QUERY_DAY_TS,
                                                   &day_resp);
            if (day_resp.alarm_arr != NULL) {
                uint32_t fc = day_resp.alarm_arr->file_count;
                int resp_len =
                    (int)(16 + sizeof(uint32_t) + fc * sizeof(PLAY_BACK_ALARM_FRAGMENT));
                char *flat = (char *)Malloc((size_t)resp_len);
                if (flat != NULL) {
                    uint32_t *p_fc;
                    memset(flat, 0, (size_t)resp_len);
                    memcpy(flat, &day_resp, 16);
                    p_fc = (uint32_t *)(flat + 16);
                    *p_fc = fc;
                    if (fc > 0) {
                        memcpy(flat + 16 + sizeof(uint32_t), day_resp.alarm_arr->file_arr,
                               fc * sizeof(PLAY_BACK_ALARM_FRAGMENT));
                    }
                    __p2p_session_pack_resp(pSession, pData, flat, resp_len);
                    Free(flat);
                }
                Free(day_resp.alarm_arr);
                day_resp.alarm_arr = NULL;
            } else {
                uint32_t empty[5];
                empty[0] = day_resp.channel;
                empty[1] = day_resp.year;
                empty[2] = day_resp.month;
                empty[3] = day_resp.day;
                empty[4] = 0;
                __p2p_session_pack_resp(pSession, pData, empty, (int)sizeof(empty));
            }
        } else {
            PR_ERR("[pb_query] unsupported low_cmd=%u", low);
            C2C_CMD_IO_CTRL_COM_RESP_T comResp;
            memset(&comResp, 0, sizeof(comResp));
            comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_FAILED;
            __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(comResp));
        }
        break;
    }
    case TY_C2C_CMD_IO_CTRL_PLAYBACK:
    case TY_C2C_CMD_IO_CTRL_PLAYBACK_GW:
    /*
     * EXT0 (100/101) is the speed-capable variant of the playback command and
     * carries the same layout: sub-command in low_cmd, channel first in the
     * payload, PB_START recognised by length. Handling it here keeps the App
     * from waiting on a command we used to ACK without acting on.
     */
    case TY_C2C_CMD_IO_CTRL_PLAYBACK_EXT0:
    case TY_C2C_CMD_IO_CTRL_PLAYBACK_GW_EXT0: {
        /* low_cmd: legacy TRANS_* (10..16) / MEDIA_STREAM_* / TY_CMD_IO_CTRL_VIDEO_* */
        uint32_t low = (uint32_t)pFixedHead->low_cmd;
        C2C_CMD_IO_CTRL_COM_RESP_T comResp;
        MEDIA_STREAM_EVENT_E ev = MEDIA_STREAM_NULL;
        void *args = pPayload;
        uint32_t ch = 0;
        C2C_TRANS_CTRL_PB_START pb_start;

        if (pFixedHead->length >= sizeof(uint32_t)) {
            memcpy(&ch, pPayload, sizeof(uint32_t));
        }
        memset(&comResp, 0, sizeof(comResp));
        comResp.channel = ch;
        comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_RECV;
        __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(comResp));

        /*
         * EXT0 is the speed-control variant: the payload is channel followed by
         * the requested speed, the remainder is reserved. Playback runs at 1x
         * only, so confirm that and refuse other rates rather than claiming a
         * rate change that never happens.
         */
        if (pFixedHead->high_cmd == TY_C2C_CMD_IO_CTRL_PLAYBACK_EXT0 ||
            pFixedHead->high_cmd == TY_C2C_CMD_IO_CTRL_PLAYBACK_GW_EXT0) {
            uint32_t speed = 0;

            if (pFixedHead->length >= 2 * sizeof(uint32_t)) {
                memcpy(&speed, (const uint8_t *)pPayload + sizeof(uint32_t), sizeof(speed));
            }
            comResp.result = (speed == 1) ? TY_C2C_CMD_IO_CTRL_COMMAND_SUCCESS : TY_C2C_CMD_IO_CTRL_COMMAND_FAILED;
            __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(comResp));
            PR_NOTICE("playback speed request x%u -> %s", speed, (speed == 1) ? "ok" : "unsupported");
            break;
        }

        if (low == 10 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_START_TS || low == (uint32_t)TY_CMD_IO_CTRL_VIDEO_PLAY ||
            low == (uint32_t)TY_CMD_IO_CTRL_VIDEO_PLAY_V2) {
            /*
             * The request the App actually sends is 20 bytes and does not match
             * C2C_TRANS_CTRL_PB_START: the time section sits one dword further
             * in, and type/reqId/allow_encrypt are absent. Confirmed against a
             * real request whose segment was [1786411126,1786411152]:
             *
             *   +0  channel
             *   +4  reserved (observed 0)
             *   +8  time_sect.start_timestamp
             *   +12 time_sect.end_timestamp
             *   +16 playTime, i.e. where the user scrubbed to
             *
             * Normalise it into the documented struct here, where the payload
             * length is known, so the app layer reads named fields instead of
             * guessing at raw offsets. A payload of any other size is dumped
             * once so a new App layout shows itself instead of being parsed
             * into a wrong seek position.
             */
            enum {
                PB_START_OFF_START = 8,
                PB_START_OFF_END = 12,
                PB_START_OFF_PLAY = 16,
                PB_START_WIRE_LEN = 20,
            };

            if (pFixedHead->length >= PB_START_WIRE_LEN) {
                const uint8_t *wire = (const uint8_t *)pPayload;

                memset(&pb_start, 0, sizeof(pb_start));
                pb_start.channel = ch;
                memcpy(&pb_start.time_sect.start_timestamp, wire + PB_START_OFF_START, sizeof(uint32_t));
                memcpy(&pb_start.time_sect.end_timestamp, wire + PB_START_OFF_END, sizeof(uint32_t));
                memcpy(&pb_start.playTime, wire + PB_START_OFF_PLAY, sizeof(uint32_t));
                pb_start.reqId = (uint32_t)pCmd->reqId;
                args = &pb_start;
            } else {
                PR_ERR("pb START payload too short: len=%d", (int)pFixedHead->length);
                PR_HEXDUMP_ERR("pb START raw", (uint8_t *)pPayload, (int)pFixedHead->length);
            }
            if (pFixedHead->length != PB_START_WIRE_LEN) {
                static unsigned s_pb_start_odd_cnt;

                if (s_pb_start_odd_cnt < 3) {
                    s_pb_start_odd_cnt++;
                    PR_WARN("pb START unexpected len=%d (expect %d)", (int)pFixedHead->length, PB_START_WIRE_LEN);
                    PR_HEXDUMP_WARN("pb START raw", (uint8_t *)pPayload, (int)pFixedHead->length);
                }
            }
            PR_NOTICE("session[%d]video pb_video_start", pSession->session);
            pSession->video_req_id = pCmd->reqId;
            memcpy(&pSession->pb_resp_head, pData, sizeof(pSession->pb_resp_head));
            if (pSession->cmd & P2P_VIDEO) {
                (void)__p2p_session_trans_video_stop(pSession);
            }
            /* clear_send deferred to demo on real (re)start; ignore path must not flush */
            pSession->cmd = (P2P_CMD_E)(pSession->cmd | P2P_PB_VIDEO);
            ev = MEDIA_STREAM_PLAYBACK_START_TS;
        } else if (low == 11 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_PAUSE ||
                   low == (uint32_t)TY_CMD_IO_CTRL_VIDEO_PAUSE) {
            pSession->cmd = (P2P_CMD_E)(pSession->cmd | P2P_PB_PAUSE);
            ev = MEDIA_STREAM_PLAYBACK_PAUSE;
        } else if (low == 12 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_RESUME ||
                   low == (uint32_t)TY_CMD_IO_CTRL_VIDEO_RESUME) {
            pSession->cmd = (P2P_CMD_E)((pSession->cmd | P2P_PB_VIDEO) & ~P2P_PB_PAUSE);
            ev = MEDIA_STREAM_PLAYBACK_RESUME;
        } else if (low == 15 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_STOP ||
                   low == (uint32_t)TY_CMD_IO_CTRL_VIDEO_STOP) {
            pSession->cmd = (P2P_CMD_E)(pSession->cmd & ~(P2P_PB_VIDEO | P2P_PB_PAUSE | P2P_PB_AUDIO));
            ev = MEDIA_STREAM_PLAYBACK_STOP;
        } else if (low == 13 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_MUTE) {
            ev = MEDIA_STREAM_PLAYBACK_MUTE;
        } else if (low == 14 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_UNMUTE) {
            ev = MEDIA_STREAM_PLAYBACK_UNMUTE;
        } else if (low == 16 || low == (uint32_t)MEDIA_STREAM_PLAYBACK_SET_SPEED) {
            ev = MEDIA_STREAM_PLAYBACK_SET_SPEED;
        } else if (low == (uint32_t)TY_CMD_IO_CTRL_AUDIO_MIC_START) {
            /* App PB accompaniment start: allow audio channel flag */
            pSession->cmd = (P2P_CMD_E)(pSession->cmd | P2P_PB_AUDIO);
            pSession->audio_req_id = pCmd->reqId;
            PR_NOTICE("client request high_cmd:[%d], operation:[%d], reqId:[%d]",
                      (int)pFixedHead->high_cmd, (int)low, pCmd->reqId);
            break;
        } else if (low == (uint32_t)TY_CMD_IO_CTRL_AUDIO_MIC_STOP) {
            pSession->cmd = (P2P_CMD_E)(pSession->cmd & ~P2P_PB_AUDIO);
            PR_NOTICE("client request high_cmd:[%d], operation:[%d], reqId:[%d]",
                      (int)pFixedHead->high_cmd, (int)low, pCmd->reqId);
            break;
        } else {
            /*
             * Only COMMAND_RECV has been sent at this point, so a bare break
             * would leave the App waiting for a final status forever. Report a
             * terminal failure and dump the header so the sub-command space of
             * the EXT0 variants can be identified.
             */
            PR_ERR("unsupported playback op: high=%d low=%d len=%d", (int)pFixedHead->high_cmd, (int)low,
                   (int)pFixedHead->length);

            comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_FAILED;
            __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(comResp));
            break;
        }
        PR_NOTICE("client request high_cmd:[%d], operation:[%d], reqId:[%d]", (int)pFixedHead->high_cmd,
                  (int)low, pCmd->reqId);
        (void)tuya_ipc_media_stream_event_call(0, (int)ch, ev, args);
        break;
    }
    default: {
        /* Newer App may send cmds we don't implement yet.
         * Returning INVALID maps to App -20001 and often aborts preview UI
         * even if live video already started. ACK SUCCESS and log for analysis.
         */
        PR_ERR("this high cmd [%d] is not support!", (int)pFixedHead->high_cmd);
        C2C_CMD_IO_CTRL_COM_RESP_T comResp;
        memset(&comResp, 0x00, sizeof(comResp));
        comResp.channel = 0;
        comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_SUCCESS;
        __p2p_session_pack_resp(pSession, pData, &comResp, sizeof(C2C_CMD_IO_CTRL_COM_RESP_T));
        break;
    }
    }

    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_session_cmd_parse
 *  Note:Session command parsing
 *  Input:
 *  Output: none
 *  Return:
 ***********************************************************/
static int __p2p_session_cmd_parse(P2P_SESSION_T *pSession, void *pData)
{
    P2P_CMD_PARSE_T *pCmd = NULL;
    C2C_CMD_FIXED_HEADER_T *pFixedHead = NULL;

    if (NULL == pSession || NULL == pData) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    pCmd = (P2P_CMD_PARSE_T *)pData;
    pFixedHead = &pCmd->str_header;

    if (0 == pFixedHead->type) {
        // Receive command request, this machine acts as server
        return __p2p_session_cmd_parse_server(pSession, pData);
    } else if (1 == pFixedHead->type) {
        // Receive response packet, this machine acts as client
        // return __p2p_session_cmd_parse_client(pSession, pData);
    } else {
        PR_ERR("pFixedHead->type error %d", pFixedHead->type);
    }

    return OPRT_COM_ERROR;
}

static int __p2p_read_cmd(P2P_SESSION_T *pSession)
{
    int ret = 0;
    C2C_CMD_FIXED_HEADER_T *pFixedHeader = NULL;
    P2P_DATA_PARSE_T *pDataParse = &pSession->proto_parse;
    P2P_CMD_PARSE_T *pReadBuff = (P2P_CMD_PARSE_T *)(pDataParse->read_buff);
    int32_t recv_len = (int32_t)pDataParse->read_size;
    ret = tuya_p2p_rtc_recv_data(pSession->session, TUYA_CMD_CHANNEL, pDataParse->read_buff + pDataParse->cur_read,
                                 &recv_len, P2P_RECV_TIMEOUT);
    pDataParse->read_size = (int)recv_len;
    if ((ret < 0) && (ERROR_P2P_TIME_OUT != ret)) {
        // Exception handling
        if (ERROR_P2P_SESSION_CLOSED_REMOTE == ret || ERROR_P2P_SESSION_CLOSED_TIMEOUT == ret ||
            ERROR_P2P_SESSION_CLOSED_CALLED == ret || ERROR_P2P_NOT_INITIALIZED == ret ||
            ERROR_P2P_INVALID_SESSION_HANDLE == ret || ERROR_P2P_INVALID_PARAMETER == ret) {
            // Session was disconnected by client, need to close session
            PR_ERR("session[%d] was close by client ret[%d]", pSession->session, ret);
            return -1;
        } else {
            // Other exceptions to be added later
            PR_ERR("session[%d] ###### error ret = [%d]", pSession->session, ret);
            return -2;
        }
    } else {
        // PR_DEBUG("recv cmd size[%d] cur_read[%d]
        // flag[%d]",pDataParse->read_size,pDataParse->cur_read,pDataParse->flag); Receive data parsing, confirm data
        // integrity
        if (READ_HEADER_PART == pDataParse->flag) {
            if (P2P_CMD_HEAD_LEN == (pDataParse->read_size + pDataParse->cur_read)) {
                // Header information read successfully, simple parsing
                if (P2P_CMD_MARK != pReadBuff->mark) {
                    // Header parsing exception, exception handling to be completed later (unlikely to reach this
                    // condition)
                    PR_ERR("session[%d] read data error mark[0x%x]", pSession->session, pReadBuff->mark);
                }
                // Extract data portion
                pFixedHeader = &(pReadBuff->str_header);
                pDataParse->read_size = pFixedHeader->length;
                pDataParse->cur_read = P2P_CMD_HEAD_LEN;
                pDataParse->flag = READ_PAYLOAD_PART;
                // PR_DEBUG("recv session[%d] cmd size[%d]",pSession->session,pDataParse->read_size);
            } else {
                // Continue extracting data to ensure header information is complete
                if (P2P_CMD_HEAD_LEN < (pDataParse->read_size + pDataParse->cur_read)) {
                    PR_ERR("session[%d] read data error", pSession->session);
                    // note Exception handling
                    // end
                    return -3;
                }
                pDataParse->cur_read = pDataParse->read_size;
                pDataParse->read_size = P2P_CMD_HEAD_LEN - pDataParse->cur_read;
            }
        } else {
            pFixedHeader = &(pReadBuff->str_header);
            if (pDataParse->read_size + pDataParse->cur_read == pFixedHeader->length + P2P_CMD_HEAD_LEN) {
                // Data reception complete, enter parsing entry
                //  PR_DEBUG("session[%d] read data succsess len[%d]",pSession->session,read_size + cur_read);
                __p2p_session_cmd_parse(pSession, pDataParse->read_buff);
                memset(pDataParse->read_buff, 0x00, SIZEOF(pDataParse->read_buff));
                pDataParse->read_size = P2P_CMD_HEAD_LEN;
                pDataParse->cur_read = 0;
                pDataParse->flag = READ_HEADER_PART;
            } else if (pDataParse->read_size + pDataParse->cur_read < pFixedHeader->length + P2P_CMD_HEAD_LEN) {
                pDataParse->cur_read += pDataParse->read_size;
                pDataParse->read_size = pFixedHeader->length + P2P_CMD_HEAD_LEN - pDataParse->cur_read;
            } else {
                PR_ERR("session[%d] read data error", pSession->session);
                // note Exception handling
                // end
                return -4;
            }
        }
    }

    return 0;
}

static void __p2p_cmd_recv_proc(void *pArg)
{
    P2P_SESSION_T *pSession = NULL;
    int ret;

    memset(&sg_p2p_session->proto_parse, 0x00, sizeof(sg_p2p_session->proto_parse));
    sg_p2p_session->proto_parse.read_size = P2P_CMD_HEAD_LEN;
    sg_p2p_session->proto_parse.flag = READ_HEADER_PART;
    while (tal_thread_get_state(sg_p2p_session->cmd_recv_proc_thread) == THREAD_STATE_RUNNING) {
        if (P2P_SESSION_IDLE == sg_p2p_session->status) {
            tal_system_sleep(5);
            continue;
        }
        pSession = sg_p2p_session;
        tal_mutex_lock(pSession->cmutex);
        if (P2P_SESSION_CLOSING == pSession->status) {
            tal_mutex_unlock(pSession->cmutex);
            tal_system_sleep(5);
            continue;
        }
        if (P2P_SESSION_RUNNING != pSession->status) {
            /* Yield like the other not-ready branches do. Spinning here burns a
             * core for the whole authentication handshake, which is exactly
             * when the listener needs the CPU. */
            tal_mutex_unlock(pSession->cmutex);
            tal_system_sleep(5);
            continue;
        }
        tal_mutex_unlock(pSession->cmutex);

        ret = __p2p_read_cmd(pSession);
        if (0 != ret) {
            PR_ERR("session[%d] read cmd failed [%d]", pSession->session, ret);
            PR_ERR("read data failed ret[%d] session[%d]", ret, sg_p2p_session->session);
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#endif
            __p2p_session_clear(pSession);
            //__p2p_wait_concurr_idle(pSession, WAIT_ALL_BUF);
            __p2p_session_release_va(pSession);
            tuya_p2p_rtc_notify_exit();
        }
    }

    PR_DEBUG("session cmd proc exit");

    return;
}

/***********************************************************
 *  Function: __p2p_video_send_proc
 *  Note:Video data transmission thread
 *  Input:
 *  Output: none
 *  Return:
 ***********************************************************/
static void __p2p_media_send_proc(void *pArg)
{
    int index = 0;
    uint32_t runCnt = 0;
    P2P_SESSION_T *pSession = NULL;
    OPERATE_RET op_ret = -1;

    PR_DEBUG("into p2p video send");

    while (tal_thread_get_state(sg_p2p_session->video_send_proc_thread) == THREAD_STATE_RUNNING) {
        if (runCnt % 2000 == 0) {
            PR_DEBUG("media send proc alive [%d]", runCnt);
        }
        runCnt++;

        if (P2P_SESSION_IDLE == sg_p2p_session->status) {
            tal_system_sleep(5);
            continue;
        }

        pSession = sg_p2p_session;
        tal_mutex_lock(pSession->cmutex);
        int status = pSession->status;
        P2P_CMD_E cmd = pSession->cmd;

        if (P2P_SESSION_CLOSING == pSession->status) {
            tal_mutex_unlock(pSession->cmutex);
            tal_system_sleep(5);
            continue;
        }
        if (P2P_SESSION_RUNNING != status) {
            /* Yield like the neighbouring not-ready branches. Spinning here
             * burns a core for the whole INITING window, which is exactly when
             * the listener and authentication need the CPU. */
            tal_mutex_unlock(pSession->cmutex);
            tal_system_sleep(5);
            continue;
        }

        // The judgment when both are not opened should be placed at the end, otherwise it will appear: users close
        // audio and video at the same time, but do not release resources This judgment cannot be omitted, otherwise
        // thread idle running will occur
        if (!(P2P_VIDEO & cmd) && !(P2P_AUDIO & cmd)) {
            // pSession->p2p_buff_stat.live_video = P2P_BUFF_IDLE;
            // pSession->p2p_buff_stat.live_audio = P2P_BUFF_IDLE;
            tal_mutex_unlock(pSession->cmutex);
            tal_system_sleep(5);
            continue;
        }
        tal_mutex_unlock(pSession->cmutex);

        /* Once per window, turn what the transport queue has been doing into a
         * bitrate the encoder can actually meet. */
        if ((P2P_VIDEO & cmd) && !(P2P_PB_VIDEO & cmd)) {
            __p2p_rate_control(pSession);
        }

        /*
         * One video frame, then up to two 20 ms audio frames. Official TuyaOpen
         * does video first. Two audio pulls keep 50 Hz capture matched to a
         * ~25 Hz send loop; a single 20 ms pull would drain at half rate.
         */
        uint32_t yield_ms = 0;

        if ((P2P_VIDEO & cmd) && !(P2P_PB_VIDEO & cmd)) {
            if (sg_p2p_session->on_get_video_frame_callback == NULL) {
                yield_ms = 10;
            } else {
                MEDIA_FRAME *pMediaFrame = &sg_p2p_session->media_frame;
                OPERATE_RET  buf_ret;

                if (!pSession->video_frame_pending) {
                    op_ret = sg_p2p_session->on_get_video_frame_callback(pMediaFrame);
                    if (op_ret != OPRT_OK) {
                        yield_ms = 10;
                    } else {
                        pSession->video_frame_pending = TRUE;
                    }
                }

                if (pSession->video_frame_pending) {
                    pSession->v_pts = (pMediaFrame->pts == 0) ? pMediaFrame->timestamp * 1000 : pMediaFrame->pts;
                    pSession->v_timestamp = pMediaFrame->timestamp;
                    if (eVideoIFrame == pMediaFrame->type) {
                        pSession->key_frame = TRUE;
                        pSession->video_need_iframe = FALSE;
                    } else {
                        pSession->key_frame = FALSE;
                    }
                    if (TRUE == pSession->video_need_iframe) {
                        pSession->video_frame_pending = FALSE;
                        /*
                         * Waiting for a key frame and this is not one. Ask for it
                         * only once the backlog has actually gone, so the largest
                         * frame the encoder makes arrives into a queue with room
                         * for it rather than on top of the congestion it is meant
                         * to recover from. Rate limited inside as well, since this
                         * runs for every frame discarded while waiting.
                         *
                         * Polled here rather than read: no frame is offered while
                         * discarding, so nothing else moves this number.
                         */
                        (void)__p2p_video_fill_pct(0, NULL, NULL);
                        if (pSession->tx_fill_pct < P2P_TX_DRAINED_PCT) {
                            (void)__p2p_request_i_frame(pSession);
                        }
                    } else {
                        /*
                         * Nothing queued behind a key frame survives it: the receiver
                         * resynchronises on the key frame and decodes none of it. KCP
                         * would still retransmit every lost segment of it, which on a
                         * link this poor is most of the budget spent on frames that can
                         * never be shown. Only what the peer has not been told about is
                         * dropped, so the sequence stays intact.
                         */
                        if (pSession->key_frame && pSession->tx_fill_pct >= P2P_TX_DRAINED_PCT) {
                            uint32_t dropped = 0;
                            if (tuya_p2p_rtc_drop_unsent(pSession->session, TUYA_VDATA_CHANNEL, &dropped) == 0 &&
                                dropped > 0) {
                                if ((pSession->tx_drop_cnt % 20) == 0) {
                                    PR_DEBUG("key frame shed %u bytes of stale backlog (queue %u pct)", dropped,
                                             pSession->tx_fill_pct);
                                }
                                pSession->tx_drop_cnt++;
                            }
                        }

                        buf_ret = __p2p_check_free_buffer_size(index, TUYA_VDATA_CHANNEL, (int)pMediaFrame->size);
                        if (buf_ret != OPRT_OK) {
                            /*
                             * Backlog is over budget: stop feeding and let it drain.
                             *
                             * Deliberately no key frame request here. A key frame is
                             * the largest frame there is - on a 300 kbps link a 30 kB
                             * one is most of a second's budget - so asking for it while
                             * the queue is already too deep adds the biggest possible
                             * object to the thing being drained. Measured: the encoder
                             * put out 1200-1400 kbps against a 256 kbps target that
                             * way, and most of that congestion was self-inflicted. The
                             * request happens below instead, once there is room.
                             */
                            pSession->video_need_iframe = TRUE;
                            pSession->video_frame_pending = FALSE;
                            yield_ms = 200;
                        } else if (TY_AV_CODEC_VIDEO_H265 != sg_p2p_session->av_Info.video_codec[0]) {
                            op_ret = __p2p_pack_h264_rtp_and_send(index, (char *)pMediaFrame->data,
                                                                 (int)pMediaFrame->size);
                        } else {
                            op_ret = __p2p_pack_h265_rtp_and_send(index, (char *)pMediaFrame->data,
                                                                 (int)pMediaFrame->size);
                        }
                        if (buf_ret == OPRT_OK) {
                            if (OPRT_OK == op_ret) {
                                pSession->video_frame_pending = FALSE;
                                pSession->dbg_vsend_ok++;
                                if ((pSession->dbg_vsend_ok % 100) == 0) {
                                    PR_DEBUG("session send video cnt [%d]", (int)pSession->dbg_vsend_ok);
                                }
                                /*
                                 * No pacing sleep here on purpose. The frame source already
                                 * sets the rate: on_get_video_frame_callback fails when the
                                 * ring is empty and that path sleeps. Sleeping a frame
                                 * period after every send caps the drain rate at exactly the
                                 * capture rate, so any backlog - a large I-frame, a stalled
                                 * send window - can never be worked off and the reader keeps
                                 * falling behind until the ring skips frames.
                                 */
                            } else {
                                pSession->dbg_vsend_fail++;
                                if ((pSession->dbg_vsend_fail % 10) == 1) {
                                    PR_ERR("video send failed count = [%u] ret=%d", pSession->dbg_vsend_fail, op_ret);
                                }
                                /*
                                 * Only a full send queue is worth retrying the same frame
                                 * for. Everything else - an oversized access unit, a frame
                                 * that is not valid Annex-B - fails identically no matter
                                 * how often it is offered, and keeping it pending retries
                                 * it forever: the stream stops for good on a single bad
                                 * frame. Drop it and take the next one instead.
                                 */
                                if (OPRT_RESOURCE_NOT_READY != op_ret) {
                                    PR_WARN("dropping unsendable frame (%u bytes, ret %d)",
                                            (uint32_t)pMediaFrame->size, op_ret);
                                    pSession->video_frame_pending = FALSE;
                                }
                                yield_ms = 200;
                            }
                        }
                    }
                }
            }
        }

        if ((P2P_AUDIO & cmd) && sg_p2p_session->on_get_audio_frame_callback != NULL) {
            MEDIA_FRAME *pAudioFrame = &sg_p2p_session->media_audio_frame;
            int          a;

            for (a = 0; a < 2; a++) {
                op_ret = sg_p2p_session->on_get_audio_frame_callback(pAudioFrame);
                if (op_ret != OPRT_OK) {
                    break;
                }
                /*
                 * Take it off the app's ring either way, so what resumes
                 * once the queue drains is current speech rather than the
                 * next of the backlog.
                 */
                if (__p2p_audio_over_budget()) {
                    pSession->dbg_ashed++;
                } else {
                    pSession->a_pts = (pAudioFrame->pts == 0) ? pAudioFrame->timestamp * 1000 : pAudioFrame->pts;
                    pSession->a_timestamp = pAudioFrame->timestamp;
                    /*
                     * Read per frame, not once at thread start. This thread is
                     * created 39 lines before av_Info is filled in the same
                     * function, and reading the codec as its first act won that
                     * race every time: the value was the zero the session was
                     * memset to, which matches none of the arms below, so every
                     * frame the ring handed over was dropped without a word.
                     * Measured on hardware: 301 frames pulled, kcp_in on the
                     * audio channel flat at zero for the whole session, and the
                     * phone heard nothing from the camera.
                     */
                    TY_AV_CODEC_ID type = sg_p2p_session->av_Info.audio_codec;

                    if (TY_AV_CODEC_AUDIO_AAC_ADTS == type) {
                        /* AAC path unused on this demo */
                    } else if (TY_AV_CODEC_AUDIO_G711A == type || TY_AV_CODEC_AUDIO_G711U == type ||
                               TY_AV_CODEC_AUDIO_PCM == type) {
                        OPERATE_RET a_ret;

                        a_ret = __p2p_pack_g711_rtp_and_send(index, (char *)pAudioFrame->data, pAudioFrame->size, type);
                        if (OPRT_OK != a_ret) {
                            pSession->dbg_asend_fail++;
                        }
                    } else {
                        pSession->dbg_asend_fail++;
                        if ((pSession->dbg_asend_fail % 50) == 1) {
                            PR_ERR("audio codec 0x%x unhandled, frame dropped (count %u)", (unsigned)type,
                                   pSession->dbg_asend_fail);
                        }
                    }
                }
            }
        }

        if (yield_ms != 0) {
            tal_system_sleep(yield_ms);
        } else if (!(P2P_VIDEO & cmd)) {
            tal_system_sleep(10);
        }
    } // while

    PR_ERR("video send task exit");
    return;
}

int __p2p_session_clear(P2P_SESSION_T *pSession)
{
    __p2p_session_all_stop(pSession);
    return 0;
}

/***********************************************************
 *  Function: __p2p_session_all_stop
 *  Note:Close all enabled functions
 *  Input:pSession Session management
 *  Output: none
 *  Return:
 ***********************************************************/
int __p2p_session_all_stop(P2P_SESSION_T *pSession)
{
    BOOL_T video_was_on = FALSE;

    /* Checked before the lock, not after: taking pSession->cmutex is itself a
     * dereference, so the old order faulted on exactly the input it meant to
     * reject - and then faulted again on the way out. */
    if (NULL == pSession) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(pSession->cmutex);
    if (P2P_VIDEO & pSession->cmd) {
        pSession->cmd &= ~P2P_VIDEO;
        video_was_on = TRUE;
    }
    if (P2P_AUDIO & pSession->cmd) {
        pSession->cmd &= ~P2P_AUDIO;
    }
    if ((P2P_PB_VIDEO & pSession->cmd) || (P2P_PB_PAUSE & pSession->cmd)) {
        pSession->cmd &= ~P2P_PB_VIDEO;
    }
    tal_mutex_unlock(pSession->cmutex);
    /* MQTT/ICE teardown never sends SPEAKER_STOP; reap the downlink thread here. */
    __p2p_audio_downlink_stop(pSession, 0);
    if (video_was_on && pSession->on_live_video_stop_callback) {
        (void)pSession->on_live_video_stop_callback();
    }
    return OPRT_OK;
}

int __p2p_session_release_va(P2P_SESSION_T *pSession)
{
    tuya_p2p_rtc_disconnect_cb_t notify;

    // All functions closed
    PR_DEBUG("release va session[%d]", pSession->session);
    tal_mutex_lock(pSession->cmutex);
    /*
     * The RTP staging buffers deliberately outlive the session.
     *
     * The sender packs and transmits with the session mutex released - it has
     * to, since a frame takes milliseconds to push out and holding the lock
     * that long would stall command handling. Freeing the buffers here, from
     * whichever thread saw the peer go away, therefore pulls memory out from
     * under a memcpy that is already running. They are P2P_RTP_PACK_LEN each,
     * so keeping them for the life of the process costs a couple of kilobytes
     * and removes the race outright; p2p_prepare_*_send_resource already
     * returns early when they are present, so a reconnect simply reuses them.
     */
    pSession->cur_clarity = TY_VIDEO_CLARITY_INNER_HIGH;
    pSession->status = P2P_SESSION_IDLE;
    pSession->cmd = P2P_IDLE;
    memset(&pSession->pb_resp_head, 0, sizeof(pSession->pb_resp_head));
    pSession->video_seq_num = 0;
    pSession->audio_seq_num = 0;
    pSession->key_frame = false;
    pSession->v_pts = 0;
    pSession->v_timestamp = 0;
    pSession->a_pts = 0;
    pSession->a_timestamp = 0;
    pSession->video_req_id = 0;
    pSession->audio_req_id = 0;
    /*
     * Keep media_frame / media_audio_frame buffers across reconnect (allocated once in
     * p2p_init via Malloc/PSRAM). Only wipe payload bookkeeping on release_va.
     */
    memset(&pSession->proto_parse, 0, sizeof(pSession->proto_parse));
    /* Keep av_Info: device static encode params (align OS — do not wipe across reconnect) */
    pSession->video_need_iframe = FALSE;
    pSession->video_frame_pending = FALSE;
    pSession->dbg_vsend_ok = 0;
    pSession->dbg_vsend_skip = 0;
    pSession->dbg_vget_fail = 0;
    notify                        = pSession->on_disconnect_callback;
    tal_mutex_unlock(pSession->cmutex);

    /* Outside the lock: this runs application code, which is free to call back
     * into P2P and would deadlock against the mutex we were holding. */
    if (notify) {
        notify();
    }
    return 0;
}

OPERATE_RET p2p_init(const TUYA_IPC_P2P_VAR_T *p_var)
{
    OPERATE_RET ret = OPRT_OK;

    /*
     * Refuse a second bring-up rather than overwrite the live session pointer.
     * The threads started by the first call read this same global on every
     * iteration, so replacing it hands them a session that is still being
     * filled in - they were seen reading a NULL thread handle out of the fresh
     * allocation and exiting - while the previous mutex, thread handles and
     * 300 kB frame buffer become unreachable.
     */
    if (NULL != sg_p2p_session) {
        PR_ERR("p2p already initialised, ignoring re-init");
        return OPRT_COM_ERROR;
    }

    // Initialize session information
    sg_p2p_session = (P2P_SESSION_T *)Malloc(sizeof(P2P_SESSION_T));
    if (NULL == sg_p2p_session) {
        PR_ERR("malloc p2p session failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(sg_p2p_session, 0, sizeof(P2P_SESSION_T));
    tal_mutex_create_init(sg_p2p_session->cmutex);
    // Get password and other verification information
    memset(&(sg_p2p_session->str_P2p_auth), 0x00, sizeof(TUYA_IPC_P2P_AUTH_T));
    tuya_ipc_get_p2p_auth(&(sg_p2p_session->str_P2p_auth));
    tuya_ipc_check_p2p_auth_update();

    sg_p2p_session->cur_clarity = TY_VIDEO_CLARITY_INNER_HIGH;

    // Start media-related threads
    THREAD_CFG_T thrd_param = {0};
    thrd_param.priority = THREAD_PRIO_2;
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    thrd_param.psram_mode = 1; /* Align OS P2P threads in PSRAM */
#endif
    thrd_param.stackDepth = STACK_SIZE_P2P_CMD_RECV;
    thrd_param.thrdname = (char *)"p2p_cmd_recv";
    ret = tal_thread_create_and_start(&(sg_p2p_session->cmd_recv_proc_thread), NULL, NULL, __p2p_cmd_recv_proc, NULL,
                                      &thrd_param);
    if (ret != OPRT_OK) {
        PR_ERR("create p2p_cmd_recv task failed");
        goto RET;
    }
    thrd_param.stackDepth = STACK_SIZE_P2P_MEDIA_SEND;
    thrd_param.thrdname = (char *)"p2p_media_send";
    ret = tal_thread_create_and_start(&(sg_p2p_session->video_send_proc_thread), NULL, NULL, __p2p_media_send_proc,
                                      NULL, &thrd_param);
    if (ret != OPRT_OK) {
        PR_ERR("create p2p_media_send task failed");
        goto RET;
    }

    // Initialize
    int bufSize = 300 * 1024; // MAX_MEDIA_FRAME_SIZE
    // memset(&sg_p2p_session->tal_video_frame, 0, sizeof(sg_p2p_session->tal_video_frame));
    // sg_p2p_session->tal_video_frame.pbuf = (char*)malloc(bufSize);
    // sg_p2p_session->tal_video_frame.buf_size = bufSize;

    memset(&sg_p2p_session->media_frame, 0, sizeof(sg_p2p_session->media_frame));
    /* Use Malloc (PSRAM when ENABLE_EXT_RAM) — 300KB must not hit SRAM */
    sg_p2p_session->media_frame.data = (uint8_t *)Malloc(bufSize);
    if (sg_p2p_session->media_frame.data == NULL) {
        PR_ERR("Malloc media_frame %d failed", bufSize);
        ret = OPRT_MALLOC_FAILED;
        goto RET;
    }
    sg_p2p_session->media_frame.size = bufSize;

    bufSize = 1280;
    // memset(&sg_p2p_session->tal_audio_frame, 0, sizeof(sg_p2p_session->tal_audio_frame));
    // sg_p2p_session->tal_audio_frame.pbuf = (char*)malloc(bufSize);
    // sg_p2p_session->tal_audio_frame.buf_size = bufSize;

    memset(&sg_p2p_session->media_audio_frame, 0, sizeof(sg_p2p_session->media_audio_frame));
    sg_p2p_session->media_audio_frame.data = (uint8_t *)Malloc(bufSize);
    if (sg_p2p_session->media_audio_frame.data == NULL) {
        PR_ERR("Malloc media_audio_frame %d failed", bufSize);
        Free(sg_p2p_session->media_frame.data);
        sg_p2p_session->media_frame.data = NULL;
        ret = OPRT_MALLOC_FAILED;
        goto RET;
    }
    sg_p2p_session->media_audio_frame.size = bufSize;

    memcpy(&sg_p2p_session->av_Info, &p_var->av_info, sizeof(TRANS_IPC_AV_INFO_T));
    __p2p_sync_speak_audio_from_av(&p_var->av_info);
    sg_p2p_session->speak_req_id = -1;
    sg_p2p_session->on_disconnect_callback = p_var->on_disconnect_callback;
    sg_p2p_session->on_get_video_frame_callback = p_var->on_get_video_frame_callback;
    sg_p2p_session->on_get_audio_frame_callback = p_var->on_get_audio_frame_callback;
    sg_p2p_session->on_live_audio_start_callback = p_var->on_live_audio_start_callback;
    sg_p2p_session->on_live_audio_stop_callback = p_var->on_live_audio_stop_callback;
    sg_p2p_session->on_recv_audio_frame_callback = p_var->on_recv_audio_frame_callback;
    sg_p2p_session->on_request_i_frame_callback   = p_var->on_request_i_frame_callback;
    sg_p2p_session->on_set_video_bitrate_callback = p_var->on_set_video_bitrate_callback;
    sg_p2p_session->on_live_video_start_callback = p_var->on_live_video_start_callback;
    sg_p2p_session->on_live_video_stop_callback = p_var->on_live_video_stop_callback;

    return OPRT_OK;

RET:
    if (NULL != sg_p2p_session->p_video_rtp_buff) {
        p2p_release_video_send_resource(sg_p2p_session);
    }
    if (NULL != sg_p2p_session->p_audio_rtp_buff) {
        p2p_release_audio_send_resource(sg_p2p_session);
    }
    __p2p_thread_exit(sg_p2p_session->cmd_recv_proc_thread);
    /*
     * The media send thread is started before the frame buffers are allocated,
     * so it is running whenever one of those allocations is what failed. Only
     * cmd_recv was stopped here, leaving it to poll a session that would never
     * be completed.
     */
    __p2p_thread_exit(sg_p2p_session->video_send_proc_thread);
    /*
     * The session itself stays allocated and the pointer stays set, which the
     * guard at the top of this function turns into a refusal of any later
     * attempt. That is deliberate: tal_thread_delete only asks a thread to
     * stop, it does not wait for it, so both threads are still dereferencing
     * this global on their way out and freeing it here would be a use after
     * free. A failed bring-up is terminal for the process by design.
     */
    return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

OPERATE_RET tuya_imm_p2p_init(const TUYA_IPC_P2P_VAR_T *p_var)
{
    return p2p_init(p_var);
}

OPERATE_RET tuya_imm_p2p_all_stream_close(int close_reason)
{
    // return tuya_ipc_p2p_stream_close(close_reason);
    return 0;
}

OPERATE_RET tuya_imm_p2p_close(void)
{
    // return tuya_ipc_tranfser_close();
    return 0;
}

OPERATE_RET tuya_imm_p2p_alive_cnt()
{
    // return tuya_ipc_p2p_alive_cnt();
    return 0;
}

OPERATE_RET tuya_imm_p2p_delete_video_finish(const char *dev_id, const uint32_t client,
                                             TUYA_DOWNLOAD_DATA_TYPE type, int success)
{
    // return tuya_ipc_delete_video_finish_v2(client, type, success);
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_download_status(const char *dev_id, const uint32_t client, const uint32_t percent)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_download_is_send_over(const char *dev_id, const uint32_t client)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_download_data(const char *dev_id, const uint32_t client,
                                           TUYA_DOWNLOAD_DATA_TYPE type, const void *pHead, const char *pData)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_album_play_send_data(const char *dev_id, const uint32_t client,
                                                  const TUYA_ALBUM_PLAY_FRAME_T *p_frame)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_playback_send_video_frame(const char *dev_id, const uint32_t client,
                                                   const MEDIA_VIDEO_FRAME_T *p_video_frame)
{
    OPERATE_RET rt;
    (void)dev_id;
    (void)client;

    if (p_video_frame == NULL || p_video_frame->p_video_buf == NULL || p_video_frame->buf_len == 0) {
        return OPRT_INVALID_PARM;
    }
    if (sg_p2p_session == NULL) {
        return OPRT_SOCK_CONN_ERR;
    }
    if (!(sg_p2p_session->cmd & P2P_PB_VIDEO)) {
        return OPRT_NOT_EXIST;
    }
    if (p2p_prepare_video_send_resource(sg_p2p_session) != OPRT_OK) {
        return OPRT_MALLOC_FAILED;
    }
    /* Align LIVE path: App needs advancing time_ms + I-frame video_param ext */
    sg_p2p_session->v_timestamp = p_video_frame->timestamp;
    sg_p2p_session->v_pts =
        (p_video_frame->pts == 0) ? (p_video_frame->timestamp * 1000ULL) : p_video_frame->pts;
    sg_p2p_session->key_frame = (p_video_frame->video_frame_type == TUYA_VIDEO_FRAME_IFRAME) ? TRUE : FALSE;
    if (sg_p2p_session->key_frame) {
        IPC_STREAM_E chn = p2p_get_chn_idx(sg_p2p_session->cur_clarity);
        if (p_video_frame->width != 0) {
            sg_p2p_session->av_Info.width[chn] = (uint32_t)p_video_frame->width;
        }
        if (p_video_frame->height != 0) {
            sg_p2p_session->av_Info.height[chn] = (uint32_t)p_video_frame->height;
        }
        if (p_video_frame->fps != 0) {
            sg_p2p_session->av_Info.fps[chn] = (uint32_t)p_video_frame->fps;
        }
    }
    if (p_video_frame->video_codec == TUYA_CODEC_VIDEO_H265) {
        rt = __p2p_pack_h265_rtp_and_send(0, (char *)p_video_frame->p_video_buf, (int)p_video_frame->buf_len);
    } else {
        rt = __p2p_pack_h264_rtp_and_send(0, (char *)p_video_frame->p_video_buf, (int)p_video_frame->buf_len);
    }
    return rt;
}

OPERATE_RET tuya_imm_p2p_playback_send_audio_frame(const char *dev_id, const uint32_t client,
                                                   const MEDIA_AUDIO_FRAME_T *p_audio_frame)
{
    TY_AV_CODEC_ID type;
    (void)dev_id;
    (void)client;

    if (p_audio_frame == NULL || p_audio_frame->p_audio_buf == NULL || p_audio_frame->buf_len == 0) {
        return OPRT_INVALID_PARM;
    }
    if (sg_p2p_session == NULL) {
        return OPRT_SOCK_CONN_ERR;
    }
    if (!(sg_p2p_session->cmd & (P2P_PB_AUDIO | P2P_PB_VIDEO))) {
        return OPRT_NOT_EXIST;
    }
    if (p_audio_frame->audio_codec == TUYA_CODEC_AUDIO_G711A) {
        type = TY_AV_CODEC_AUDIO_G711A;
    } else if (p_audio_frame->audio_codec == TUYA_CODEC_AUDIO_PCM) {
        type = TY_AV_CODEC_AUDIO_PCM;
    } else {
        type = TY_AV_CODEC_AUDIO_G711U;
    }
    return __p2p_pack_g711_rtp_and_send(0, (char *)p_audio_frame->p_audio_buf, (int)p_audio_frame->buf_len, type);
}

OPERATE_RET tuya_imm_p2p_playback_send_fragment_end(const char *dev_id, const uint32_t client,
                                                    const PLAYBACK_TIME_S *fgmt)
{
    (void)dev_id;
    (void)client;
    (void)fgmt;
    PR_NOTICE("[pb] fragment_end");
    return OPRT_OK;
}

OPERATE_RET tuya_imm_p2p_playback_send_finish(const char *dev_id, const uint32_t client)
{
    (void)dev_id;
    (void)client;
    if (sg_p2p_session != NULL) {
        sg_p2p_session->cmd = (P2P_CMD_E)(sg_p2p_session->cmd & ~(P2P_PB_VIDEO | P2P_PB_AUDIO | P2P_PB_PAUSE));
    }
    PR_NOTICE("[pb] send_finish");
    return OPRT_OK;
}

OPERATE_RET tuya_ipc_media_playback_send_video_frame(const uint32_t client,
                                                     const MEDIA_VIDEO_FRAME_T *p_video_frame)
{
    return tuya_imm_p2p_playback_send_video_frame(NULL, client, p_video_frame);
}

OPERATE_RET tuya_ipc_media_playback_send_audio_frame(const uint32_t client,
                                                     const MEDIA_AUDIO_FRAME_T *p_audio_frame)
{
    return tuya_imm_p2p_playback_send_audio_frame(NULL, client, p_audio_frame);
}

OPERATE_RET tuya_ipc_media_playback_send_fragment_end(const uint32_t client, const PLAYBACK_TIME_S *fgmt)
{
    return tuya_imm_p2p_playback_send_fragment_end(NULL, client, fgmt);
}

OPERATE_RET tuya_ipc_media_playback_send_finish(const uint32_t client)
{
    return tuya_imm_p2p_playback_send_finish(NULL, client);
}

/**
 * @brief Clear P2P AV send buffers (VDATA/ADATA) for current session
 * @return none
 */
void tuya_ipc_media_p2p_clear_send(void)
{
    if (sg_p2p_session == NULL) {
        return;
    }
    (void)tuya_p2p_rtc_clear_send_buffer(sg_p2p_session->session, TUYA_VDATA_CHANNEL);
    (void)tuya_p2p_rtc_clear_send_buffer(sg_p2p_session->session, TUYA_ADATA_CHANNEL);
}

void tuya_ipc_media_p2p_drop_unsent(void)
{
    uint32_t vdrop = 0;
    uint32_t adrop = 0;

    if (sg_p2p_session == NULL) {
        return;
    }
    (void)tuya_p2p_rtc_drop_unsent(sg_p2p_session->session, TUYA_VDATA_CHANNEL, &vdrop);
    (void)tuya_p2p_rtc_drop_unsent(sg_p2p_session->session, TUYA_ADATA_CHANNEL, &adrop);
    if (vdrop != 0 || adrop != 0) {
        PR_NOTICE("pb drop unsent v=%u a=%u", vdrop, adrop);
    }
}

void tuya_ipc_media_p2p_video_send_start(void)
{
    P2P_CMD_PARSE_T head;
    C2C_CMD_IO_CTRL_COM_RESP_T comResp;
    char *sendBuff = NULL;
    int packLen;
    int ret;

    if (sg_p2p_session == NULL) {
        return;
    }

    memset(&head, 0, sizeof(head));
    if (sg_p2p_session->pb_resp_head.mark == P2P_CMD_MARK) {
        memcpy(&head, &sg_p2p_session->pb_resp_head, sizeof(head));
    } else {
        head.mark = P2P_CMD_MARK;
        head.reqId = sg_p2p_session->video_req_id;
        head.str_header.high_cmd = TY_C2C_CMD_IO_CTRL_PLAYBACK;
    }
    /* Device notify: App waits for 50 after START RECV; SUCCESS on START is "play ended". */
    head.str_header.type = 0;
    head.str_header.low_cmd = TY_CMD_IO_CTRL_VIDEO_SEND_START;
    memset(&comResp, 0, sizeof(comResp));
    comResp.channel = 0;
    comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_SUCCESS;
    head.str_header.length = (unsigned int)sizeof(comResp);

    packLen = P2P_CMD_HEAD_LEN + (int)sizeof(comResp);
    sendBuff = (char *)Malloc(packLen);
    if (sendBuff == NULL) {
        PR_ERR("pb VIDEO_SEND_START malloc failed");
        return;
    }
    memcpy(sendBuff, &head, P2P_CMD_HEAD_LEN);
    memcpy(sendBuff + P2P_CMD_HEAD_LEN, &comResp, sizeof(comResp));
    ret = tuya_p2p_rtc_send_data(sg_p2p_session->session, TUYA_CMD_CHANNEL, sendBuff, packLen, -1);
    Free(sendBuff);
    if (ret < 0) {
        PR_ERR("pb VIDEO_SEND_START send failed ret=%d reqId=%d", ret, head.reqId);
        return;
    }
    PR_NOTICE("pb VIDEO_SEND_START reqId=%d", head.reqId);
}

OPERATE_RET
tuya_imm_p2p_playback_send_video_frame_with_encrypt(const uint32_t client, uint32_t reqId,
                                                    const TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_video_frame)
{
    return 0;
}

OPERATE_RET
tuya_imm_p2p_playback_send_audio_frame_with_encrypt(const uint32_t client, uint32_t reqId,
                                                    const TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_audio_frame)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_album_play_send_finish(const char *dev_id, const uint32_t client)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *rtp_alloc(void *param, int bytes)
{
    unsigned char *pBuffer = NULL;

    if (bytes <= 0 || bytes > 4096) {
        return NULL;
    }
    pBuffer = (unsigned char *)malloc((size_t)bytes);
    if (pBuffer == NULL) {
        return NULL;
    }
    memset(pBuffer, 0, (size_t)bytes);
    return pBuffer;
}

void rtp_free(void *param, void *packet)
{
    free(packet);
    packet = NULL;
    return;
}

int rtp_pack_packet_handler(void *param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    char *buf = (char *)packet;
    int len = bytes;
    int total;
    RTP_PACK_NAL_ARG_T *nal_arg = (RTP_PACK_NAL_ARG_T *)param;

    if (nal_arg == NULL || nal_arg->p_rtp_buff == NULL || packet == NULL || len <= 0) {
        return -1;
    }
    if (nal_arg->fix_len <= 0 || nal_arg->fix_len > P2P_RTP_PACK_LEN) {
        return -1;
    }
    total = len + nal_arg->fix_len;
    if (total > P2P_RTP_PACK_LEN || total <= nal_arg->fix_len) {
        return -1;
    }
    memcpy(nal_arg->p_rtp_buff, nal_arg->ext_head_buff, nal_arg->fix_len);
    *(int *)&nal_arg->p_rtp_buff[nal_arg->fix_len - 4] = len;
    memcpy(nal_arg->p_rtp_buff + nal_arg->fix_len, buf, len);
    if (p2p_send_rtp_data(nal_arg->client, nal_arg->channel, nal_arg->p_rtp_buff, total) != OPRT_OK) {
        return -1;
    }
    nal_arg->sent_pkts++;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

int OnGetVideoFrameCallback(MEDIA_FRAME *pMediaFrame)
{
    // TAL_VENC_FRAME_T *pTalVideoFrame = &sg_p2p_session->tal_video_frame;
    // if (tal_venc_get_frame(0, 0, pTalVideoFrame) != 0)
    // {
    //     return -1;
    // }
    // memcpy(pMediaFrame->data, pTalVideoFrame->pbuf, pTalVideoFrame->used_size);
    // pMediaFrame->size = pTalVideoFrame->used_size;
    // pMediaFrame->pts = pTalVideoFrame->pts;
    // pMediaFrame->timestamp = pTalVideoFrame->timestamp;
    // pMediaFrame->type = (MEDIA_FRAME_TYPE)pTalVideoFrame->frametype;
    return 0;
}

int OnGetAudioFrameCallback(MEDIA_FRAME *pMediaFrame)
{
    // TAL_AUDIO_FRAME_INFO_T *pTalAudioFrame = &sg_p2p_session->tal_audio_frame;
    // if (tal_ai_get_frame(0, 0, pTalAudioFrame) != 0)
    // {
    //     return -1;
    // }
    // memcpy(pMediaFrame->data, pTalAudioFrame->pbuf, pTalAudioFrame->used_size);
    // pMediaFrame->size = pTalAudioFrame->used_size;
    // pMediaFrame->pts = pTalAudioFrame->pts;
    // pMediaFrame->timestamp = pTalAudioFrame->timestamp;
    // pMediaFrame->type = (MEDIA_FRAME_TYPE)pTalAudioFrame->type;
    return 0;
}

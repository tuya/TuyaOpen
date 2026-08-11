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
#define P2P_SESSION_INITING (3)

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
    BOOL_T video_frame_pending;                     // Hold last get_frame until RTP send succeeds

    tuya_p2p_rtc_disconnect_cb_t on_disconnect_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_video_frame_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_audio_frame_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_video_start_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_video_stop_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_audio_start_callback;
    tuya_p2p_rtc_live_video_cb_t on_live_audio_stop_callback;
    tuya_p2p_rtc_get_frame_cb_t  on_recv_audio_frame_callback;
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
    PR_NOTICE("p2p listen task exit");
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

P2P_SESSION_T *p2p_get_idle_session(int *index)
{
    int status = -1;
    int i = 0;
    if (sg_p2p_session == NULL)
        return NULL;
    PR_DEBUG("p2p_get_idle_session begin\n");
    status = sg_p2p_session->status;
    if (P2P_SESSION_IDLE == status) {
        *index = i;
        sg_p2p_session->status = P2P_SESSION_INITING;
        return sg_p2p_session;
    }
    PR_DEBUG("p2p_get_idle_session end\n");
    return NULL;
}

OPERATE_RET p2p_deal_with_listen(int session)
{
    OPERATE_RET ret = OPRT_OK;
    BOOL_T userCheckEnable = FALSE;

    PR_NOTICE("__p2p_deal_with_listen, session[%d]", session);

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
        __p2p_rtc_close(session, RTC_CLOSE_REASON_AUTH_FAIL, NULL);
        tuya_p2p_rtc_notify_exit();
        tuya_p2p_rtc_deinit();
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

RET:
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
    int retry = P2P_CHECK_USER_TIMES * 6 / timeout;

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
                cur_read += read_size;
                read_size = sizeof(P2P_CMD_PASSWD_T) - cur_read;
            } else {
                PR_ERR("get userinfo error session[%d]", session);
                return OPRT_COM_ERROR;
            }
        }
    } // while (retry > 0)

    if (FALSE == flag) {
        PR_ERR("get userinfo timeout session[%d]", session);
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
        return OPRT_RESOURCE_NOT_READY;
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

static OPERATE_RET __p2p_check_free_buffer_size(int client, int channel, int len)
{
    OPERATE_RET ret = OPRT_OK;
    int sendFreeSize = 0;
    int writeSize = 0;

    ret = tuya_p2p_rtc_check_buffer(sg_p2p_session->session, channel, (uint32_t *)&writeSize, NULL,
                                    (uint32_t *)&sendFreeSize);
    if (OPRT_OK != ret) {
        return ret;
    }

    int need_size = (int)((double)len * 1.1); /* align TuyaOS svc_streaming_p2p check */
    if (need_size > sendFreeSize) {
        static int retry_sum = 0; // Total retry count when buffer is full
        if (retry_sum % 100 == 0) {
            PR_ERR("Check_Buffer not enough writeSize[%d] sendFreeSize[%d] len[%d] session[%d] channel[%d]", writeSize,
                   sendFreeSize, len, sg_p2p_session->session, channel);
        }
        retry_sum++;
        ret = OPRT_RESOURCE_NOT_READY;
    }
    return ret;
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
    pRtpDelegate = rtp_payload_encode_create(/*H265_PAY_LOAD*/ 95, "H265", seq, ssrc, &rtp_packer, &rtp_pack_nal_arg);
    ret = rtp_payload_encode_input(pRtpDelegate, pData, len, timestamp);
    if (OPRT_OK != ret) {
        PR_ERR("rtp_payload_encode_input h264 error:%d", ret);
    }
    rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->video_seq_num, &timestamp);
    rtp_payload_encode_destroy(pRtpDelegate);

    return ret;
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
        PR_ERR("rtp_payload_encode_input h264 error:%d", ret);
        rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->video_seq_num, &timestamp);
        rtp_payload_encode_destroy(pRtpDelegate);
        return OPRT_RESOURCE_NOT_READY;
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
    ret = rtp_payload_encode_input(pRtpDelegate, pData, len, timestamp);
    if (OPRT_OK != ret) {
        PR_ERR("rtp_payload_encode_input h264 error:%d", ret);
    }
    rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->audio_seq_num, &timestamp);
    rtp_payload_encode_destroy(pRtpDelegate);

    return ret;
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
    if (NULL == pSession || (P2P_VIDEO & pSession->cmd)) {
        PR_ERR("param error or video started");
        return OPRT_INVALID_PARM;
    }
    // Wait for previous data transmission to end
    PR_DEBUG("session[%d]video video_start wait_concurr_idle", pSession->session);
    pSession->cmd |= P2P_VIDEO;
    pSession->video_need_iframe = TRUE;
    pSession->key_frame = FALSE;
    pSession->dbg_vsend_ok = 0;
    pSession->dbg_vsend_skip = 0;
    pSession->dbg_vget_fail = 0;
    pSession->video_frame_pending = FALSE;
    if (pSession->on_live_video_start_callback) {
        (void)pSession->on_live_video_start_callback();
    }
    (void)tuya_ipc_media_stream_event_call(0, 0, MEDIA_STREAM_LIVE_VIDEO_START, NULL);
    PR_DEBUG("session[%d] video start success (wait first I-frame)", pSession->session);
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
            pSession->audio_downlink_on = FALSE;
            pSession->cmd = (P2P_CMD_E)(pSession->cmd & ~P2P_SPEAKER);
            pSession->speak_req_id = -1;
            if (pSession->audio_downlink_thread) {
                THREAD_HANDLE h = pSession->audio_downlink_thread;
                pSession->audio_downlink_thread = NULL;
                tal_thread_delete(h);
            }
            if (pSession->on_live_audio_stop_callback) {
                (void) pSession->on_live_audio_stop_callback();
            }
            (void)tuya_ipc_media_stream_event_call(0, (int)parm->channel, MEDIA_STREAM_SPEAKER_STOP, NULL);
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
            tal_mutex_unlock(pSession->cmutex);
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
    TY_AV_CODEC_ID type;
    type = sg_p2p_session->av_Info.audio_codec;
    // type = TY_AV_CODEC_AUDIO_PCM;

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
            tal_mutex_unlock(pSession->cmutex);
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

        /*
         * Align TuyaOS push path: audio must not be starved by video sleep/backoff.
         * Drain uplink audio first (up to a few frames), then try one video frame.
         */
        if (P2P_AUDIO & cmd) {
            if (sg_p2p_session->on_get_audio_frame_callback == NULL) {
                tal_system_sleep(10);
            } else {
                int a_burst;

                for (a_burst = 0; a_burst < 4; a_burst++) {
                    MEDIA_FRAME *pAudioFrame = &sg_p2p_session->media_audio_frame;

                    op_ret = sg_p2p_session->on_get_audio_frame_callback(pAudioFrame);
                    if (op_ret != OPRT_OK) {
                        break;
                    }
                    pSession->a_pts = (pAudioFrame->pts == 0) ? pAudioFrame->timestamp * 1000 : pAudioFrame->pts;
                    pSession->a_timestamp = pAudioFrame->timestamp;
                    if (TY_AV_CODEC_AUDIO_AAC_ADTS == type) {
                        /* AAC path unused on this demo */
                    } else if (TY_AV_CODEC_AUDIO_G711A == type || TY_AV_CODEC_AUDIO_G711U == type ||
                               TY_AV_CODEC_AUDIO_PCM == type) {
                        (void) __p2p_pack_g711_rtp_and_send(index, (char *)pAudioFrame->data, pAudioFrame->size,
                                                              type);
                    }
                }
            }
        }

        if (P2P_VIDEO & cmd) {
            if (sg_p2p_session->on_get_video_frame_callback == NULL) {
                tal_system_sleep(10);
            } else {
                MEDIA_FRAME *pMediaFrame = &sg_p2p_session->media_frame;
                OPERATE_RET buf_ret;
                int video_fps;
                uint32_t pace_ms;
                uint32_t backoff_ms;

                if (!pSession->video_frame_pending) {
                    op_ret = sg_p2p_session->on_get_video_frame_callback(pMediaFrame);
                    if (op_ret != OPRT_OK) {
                        /* Short sleep only; next loop drains audio again */
                        tal_system_sleep(10);
                        continue;
                    }
                    pSession->video_frame_pending = TRUE;
                }

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
                    continue;
                }

                buf_ret = __p2p_check_free_buffer_size(index, TUYA_VDATA_CHANNEL, (int)pMediaFrame->size);
                if (buf_ret != OPRT_OK) {
                    /* Align live preview: drop until next I-frame when send queue full */
                    pSession->video_need_iframe = TRUE;
                    pSession->video_frame_pending = FALSE;
                    /* Keep audio alive while video TX is congested */
                    backoff_ms = (P2P_AUDIO & cmd) ? 20 : 200;
                    tal_system_sleep(backoff_ms);
                    continue;
                }

                if (TY_AV_CODEC_VIDEO_H265 != sg_p2p_session->av_Info.video_codec[0]) {
                    op_ret = __p2p_pack_h264_rtp_and_send(index, (char *)pMediaFrame->data, (int)pMediaFrame->size);
                } else {
                    op_ret = __p2p_pack_h265_rtp_and_send(index, (char *)pMediaFrame->data, (int)pMediaFrame->size);
                }
                if (OPRT_OK == op_ret) {
                    pSession->video_frame_pending = FALSE;
                    pSession->dbg_vsend_ok++;
                    if ((pSession->dbg_vsend_ok % 100) == 0) {
                        PR_DEBUG("session send video cnt [%d]", (int)pSession->dbg_vsend_ok);
                    }
                    video_fps = sg_p2p_session->av_Info.fps[0];
                    if (video_fps <= 0 || video_fps > 60) {
                        video_fps = 15;
                    }
                    pace_ms = (uint32_t)(1000 / video_fps);
                    if (pace_ms < 20) {
                        pace_ms = 20;
                    }
                    tal_system_sleep(pace_ms);
                } else {
                    static uint32_t s_vsend_fail_cnt = 0;
                    s_vsend_fail_cnt++;
                    if ((s_vsend_fail_cnt % 10) == 1) {
                        PR_ERR("video send failed count = [%d]", (int)s_vsend_fail_cnt);
                    }
                    backoff_ms = (P2P_AUDIO & cmd) ? 20 : 500;
                    tal_system_sleep(backoff_ms);
                }
            }
        } else if (!(P2P_AUDIO & cmd)) {
            tal_system_sleep(5);
        } else {
            /* Audio-only: brief yield when ring empty */
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

    tal_mutex_lock(pSession->cmutex);
    if (NULL == pSession) {
        PR_ERR("param error");
        tal_mutex_unlock(pSession->cmutex);
        return OPRT_INVALID_PARM;
    }
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
    if (video_was_on && pSession->on_live_video_stop_callback) {
        (void)pSession->on_live_video_stop_callback();
    }
    return OPRT_OK;
}

int __p2p_session_release_va(P2P_SESSION_T *pSession)
{
    // All functions closed
    PR_DEBUG("release va session[%d]", pSession->session);
    tal_mutex_lock(pSession->cmutex);
    if (pSession->p_video_rtp_buff) {
        Free(pSession->p_video_rtp_buff);
        pSession->p_video_rtp_buff = NULL;
    }
    if (pSession->p_audio_rtp_buff) {
        Free(pSession->p_audio_rtp_buff);
        pSession->p_audio_rtp_buff = NULL;
    }
    // memset(&pSession->session, 0x00, sizeof(P2P_SESSION_T) - OFFSET(P2P_SESSION_T, session));//Clear variables
    // outside the lock memset(&pSession->str_P2p_auth, 0, sizeof(pSession->str_P2p_auth));
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
    if (pSession->on_disconnect_callback)
        pSession->on_disconnect_callback(); // Notify upper layer when receiving disconnect signal from cloud
    tal_mutex_unlock(pSession->cmutex);
    return 0;
}

OPERATE_RET p2p_init(const TUYA_IPC_P2P_VAR_T *p_var)
{
    OPERATE_RET ret = OPRT_OK;

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
    if (sg_p2p_session == NULL || !(sg_p2p_session->cmd & P2P_PB_VIDEO)) {
        return OPRT_RESOURCE_NOT_READY;
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
    if (sg_p2p_session == NULL || !(sg_p2p_session->cmd & (P2P_PB_AUDIO | P2P_PB_VIDEO))) {
        return OPRT_RESOURCE_NOT_READY;
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

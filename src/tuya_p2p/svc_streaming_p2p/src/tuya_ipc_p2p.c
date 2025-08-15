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
#include "tuya_media_service_rtc.h"
#include "rtp-payload.h"

#define TUYA_CMD_CHANNEL        (0) // Signaling channel, signal mode refer to P2P_CMD_E
#define TUYA_VDATA_CHANNEL      (1) // Video data channel
#define TUYA_ADATA_CHANNEL      (2) // Audio data channel
#define TUYA_P2P_CMD_CHECK(cmd) (cmd == P2P_LIVE || cmd == P2P_PLAYBACK || cmd == P2P_PAUSE)
#define TUYA_P2P_CHN_CHECK(cmd) (cmd == TUYA_CMD_CHANNEL || cmd == TUYA_VDATA_CHANNEL || cmd == TUYA_ADATA_CHANNEL)

#define P2P_SESSION_IDLE    (0)
#define P2P_SESSION_RUNNING (1)
#define P2P_SESSION_CLOSING (2)
#define P2P_SESSION_INITING (3)

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

#define OFFSET(TYPE, MEMBER) ((SIZE_T)(&(((TYPE *)0)->MEMBER)))

#define STACK_SIZE_P2P_MEDIA_SEND 65536
#define STACK_SIZE_P2P_MEDIA_RECV 65536
#define STACK_SIZE_P2P_CMD_SEND   65536
#define STACK_SIZE_P2P_CMD_RECV   65536
#define STACK_SIZE_P2P_DETECT     65536
#define STACK_SIZE_P2P_LISTEN     131072

typedef struct {
    INT_T client;
    INT_T channel;
    CHAR_T *p_rtp_buff;                         // RTP data buffer, reference size MTU+100
    INT_T fix_len;                              // Supplementary private header data
    CHAR_T ext_head_buff[P2P_EXT_HEAD_MAX_LEN]; // According to extended video header protocol head+ext(8)+rtp_len
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
    INT_T read_size; // init P2P_CMD_HEAD_LEN;
    CHAR_T read_buff[P2P_CMD_PARSE_MAX_SIZE_V2];
    INT_T cur_read; // Current read length
    INT_T flag;     // READ_HEADER_PART/READ_PAYLOAD_PART
} P2P_DATA_PARSE_T;

typedef struct {
    MUTEX_HANDLE cmutex;
    TUYA_IPC_P2P_AUTH_T str_P2p_auth;
    /*******client*******/
    INT_T session; // Save session number
    INT_T status;  // Session status  0 not started
    /*******p2p server*******/
    P2P_CMD_E cmd; // Signal status information
    P2P_CMD_PARSE_T pb_resp_head;
    CHAR_T *p_video_rtp_buff; // Video RTP data buffer, reference size MTU+100
    CHAR_T *p_audio_rtp_buff; // Audio RTP data buffer, reference size MTU+100
    USHORT_T video_seq_num;   // Video RTP packet sequence number
    USHORT_T audio_seq_num;   // Audio RTP packet sequence number
    BOOL_T key_frame;
    UINT64_T v_pts;                                  // Video PTS
    UINT64_T v_timestamp;                            // Video absolute time (ms)
    UINT64_T a_pts;                                  // Audio PTS
    UINT64_T a_timestamp;                            // Audio absolute time (ms)
    INT_T video_req_id;                              // Video request ID, used for preview, playback and other services
    INT_T audio_req_id;                              // Audio request ID
    TRANSFER_VIDEO_CLARITY_TYPE_INNER_E cur_clarity; // Current video clarity type
    P2P_DATA_PARSE_T proto_parse;
    TRANS_IPC_AV_INFO_T av_Info; // TODO currently video parameters must be consistent

    tuya_p2p_rtc_disconnect_cb_t on_disconnect_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_video_frame_callback;
    tuya_p2p_rtc_get_frame_cb_t on_get_audio_frame_callback;
    THREAD_HANDLE cmd_recv_proc_thread;   // Command receive thread handle
    THREAD_HANDLE video_send_proc_thread; // Video send thread handle
    // TAL_VENC_FRAME_T tal_video_frame;
    // TAL_AUDIO_FRAME_INFO_T tal_audio_frame;
    MEDIA_FRAME media_frame;
    MEDIA_FRAME media_audio_frame;
    /******* p2p server*******/
} P2P_SESSION_T;

STATIC P2P_SESSION_T *sg_p2p_session = NULL;
INT_T g_listen_start = 0;               // Flag variable to control listen thread start or stop
THREAD_HANDLE g_listen_thrd_hdl = NULL; // Listen thread handle

OPERATE_RET p2p_deal_with_listen(INT_T session);
OPERATE_RET p2p_get_userinfo(INT_T session, INT_T p2pType);
IPC_STREAM_TYPE p2p_get_chn_idx(TRANSFER_VIDEO_CLARITY_TYPE_INNER_E cur_clarity);
TRANSFER_VIDEO_CLARITY_TYPE p2p_clarity_trans(TRANSFER_VIDEO_CLARITY_TYPE_INNER_E type);
INT_T p2p_prepare_video_send_resource(P2P_SESSION_T *pSession);
INT_T p2p_release_video_send_resource(P2P_SESSION_T *pSession);
INT_T p2p_prepare_audio_send_resource(P2P_SESSION_T *pSession);
INT_T p2p_release_audio_send_resource(P2P_SESSION_T *pSession);
INT_T __p2p_session_clear(P2P_SESSION_T *pSession);
INT_T __p2p_session_all_stop(P2P_SESSION_T *pSession);
INT_T __p2p_session_release_va(P2P_SESSION_T *pSession);
VOID __p2p_thread_exit(THREAD_HANDLE thread);
VOID __p2p_rtc_close(INT_T rtc_session, INT_T reason, P2P_SESSION_T* p2p_session);

void *rtp_alloc(void *param, int bytes);
void rtp_free(void *param, void *packet);
int rtp_pack_packet_handler(void *param, const void *packet, int bytes, uint32_t timestamp, int flags);

void ctx_listen_thread_func(void *arg)
{
    printf("listen task start\n");
    while (1) {
        INT_T session_id = tuya_p2p_rtc_listen();
        if (session_id < 0) {
            printf("listen failed session:[%d]\n", session_id);
            break;
        }
        tuya_p2p_rtc_session_info_t session_info = {0};
        tuya_p2p_rtc_get_session_info(session_id, &session_info);
        p2p_deal_with_listen(session_id);
    }
    printf("listen task exit\n");
    return;
}

OPERATE_RET p2p_rtc_listen_start()
{
    THREAD_CFG_T param;
    param.priority = THREAD_PRIO_3;
    param.stackDepth = 128 * 1024;
    param.thrdname = "tuya_p2p_listen_task";
    if (g_listen_start) {
        printf("p2p listen thread already started");
        return OPRT_COM_ERROR;
    }
    int result = tal_thread_create_and_start(&g_listen_thrd_hdl, NULL, NULL, ctx_listen_thread_func, NULL, &param);
    if (OPRT_OK != result) {
        printf("create p2p listen thread failed %d", result);
        return result;
    }
    g_listen_start = 1;
    return OPRT_OK;
}

OPERATE_RET p2p_rtc_listen_stop()
{
    if (g_listen_start != 1) {
        printf("p2p listen thread not started");
        return OPRT_COM_ERROR;
    }
    tuya_p2p_rtc_listen_break();
    tal_thread_delete(g_listen_thrd_hdl);
    g_listen_start = 0;
    return OPRT_OK;
}

P2P_SESSION_T *p2p_get_idle_session(INT_T *index)
{
    INT_T status = -1;
    INT_T i;
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

OPERATE_RET p2p_deal_with_listen(INT_T session)
{
    OPERATE_RET ret = OPRT_OK;
    BOOL_T userCheckEnable = FALSE;

    // First verify user information, close corresponding session if not qualified
    if (OPRT_OK != p2p_get_userinfo(session, 1)) {
        PR_ERR("check userinfo error");
        //__p2p_rtc_close(session, RTC_CLOSE_REASON_AUTH_FAIL, NULL);
        PR_ERR("Close session[%d] \n", session);
        if (FALSE == userCheckEnable) {
            PR_ERR("resend p2p passwd to service");
            // Resend passwd once
            if (OPRT_OK == tuya_ipc_p2p_update_pw(sg_p2p_session->str_P2p_auth.p2p_passwd)) {
                userCheckEnable = TRUE;
            }
        }
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
        goto RET;
    }
    if (OPRT_OK != (ret = p2p_prepare_audio_send_resource(sg_p2p_session))) {
        goto RET;
    }

    // Save connection information
    sg_p2p_session->session = session;
    sg_p2p_session->status = P2P_SESSION_RUNNING;

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
OPERATE_RET p2p_get_userinfo(INT_T session, INT_T p2pType)
{
    CHAR_T *read_buff = NULL;
    P2P_CMD_PASSWD_T strUserInfo;
    INT_T ret;
    INT_T cur_read = 0;
    INT_T read_size = sizeof(P2P_CMD_PASSWD_T);
    INT_T tmpSize = 0;
    BOOL_T flag = FALSE;
    INT_T timeout = P2P_RECV_TIMEOUT; // ms
    INT_T retry = P2P_CHECK_USER_TIMES * 6 / timeout;

    memset(&strUserInfo, 0x00, sizeof(P2P_CMD_PASSWD_T));
    read_buff = (CHAR_T *)&strUserInfo;

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
                if (P2P_CMD_MARK != ((P2P_CMD_PASSWD_T *)read_buff)->mark) {
                    // Header parsing exception, exception handling to be completed later (unlikely to reach this
                    // condition)
                    PR_ERR("session[%d] read data error mark[0x%x]", session, ((P2P_CMD_PASSWD_T *)read_buff)->mark);
                    return OPRT_COM_ERROR;
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
    CHAR_T sign[32 + 1] = {0};
    TKL_HASH_HANDLE md5;
    tal_md5_create_init(&md5);
    tal_md5_starts_ret(md5);
    unsigned char decrypt[16];
    tal_md5_update_ret(md5, (BYTE_T *)(sg_p2p_session->str_P2p_auth.p2p_passwd),
                       strlen(sg_p2p_session->str_P2p_auth.p2p_passwd));
    tal_md5_update_ret(md5, (BYTE_T *)"||", 2);
    tal_md5_update_ret(md5, (BYTE_T *)(sg_p2p_session->str_P2p_auth.gw_local_key),
                       strlen(sg_p2p_session->str_P2p_auth.gw_local_key));
    tal_md5_finish_ret(md5, decrypt);
    tal_md5_free(md5);

    INT_T offset = 0;
    INT_T i = 0;
    for (i = 0; i < 16; i++) {
        sprintf(&sign[offset], "%02x", decrypt[i]);
        offset += 2;
    }
    sign[offset] = 0;

    if (strcmp(strUserInfo.user, sg_p2p_session->str_P2p_auth.p2p_name) == 0 && strcmp(strUserInfo.passwd, sign) == 0) {
        PR_DEBUG("auth success");
        return OPRT_OK;
    }

    CHAR_T lk_dm5[32 + 1] = {0};
    tal_md5_create_init(&md5);
    tal_md5_starts_ret(md5);
    tal_md5_update_ret(md5, (BYTE_T *)(sg_p2p_session->str_P2p_auth.gw_local_key),
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

VOID __p2p_thread_exit(THREAD_HANDLE thread)
{
    if (NULL != thread) {
        tal_thread_delete(thread);
    }
    return;
}

VOID __p2p_rtc_close(INT_T rtc_session, INT_T reason, P2P_SESSION_T* p2p_session)
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

INT_T p2p_prepare_video_send_resource(P2P_SESSION_T *pSession)
{
    if (pSession == NULL) {
        PR_DEBUG("session is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL != pSession->p_video_rtp_buff) {
        return OPRT_OK;
    }

    pSession->p_video_rtp_buff = (CHAR_T *)Malloc(P2P_RTP_PACK_LEN);
    if (NULL == pSession->p_video_rtp_buff) {
        PR_ERR("session:[%d] video rtp buffer malloc failed", pSession->session);
        return OPRT_MALLOC_FAILED;
    }
    memset(pSession->p_video_rtp_buff, 0x00, P2P_RTP_PACK_LEN);

    PR_DEBUG("session:[%d] malloc video send buffer success", pSession->session);
    return OPRT_OK;
}

INT_T p2p_release_video_send_resource(P2P_SESSION_T *pSession)
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

INT_T p2p_prepare_audio_send_resource(P2P_SESSION_T *pSession)
{
    if (pSession == NULL) {
        PR_DEBUG("session is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NULL != pSession->p_audio_rtp_buff) {
        return OPRT_OK;
    }

    pSession->p_audio_rtp_buff = (CHAR_T *)Malloc(P2P_RTP_PACK_LEN);
    if (NULL == pSession->p_audio_rtp_buff) {
        PR_ERR("session:[%d] audio rtp buffer malloc failed", pSession->session);
        return OPRT_MALLOC_FAILED;
    }
    memset(pSession->p_audio_rtp_buff, 0x00, P2P_RTP_PACK_LEN);

    PR_DEBUG("session:[%d] malloc audio send buffer success", pSession->session);
    return OPRT_OK;
}

INT_T p2p_release_audio_send_resource(P2P_SESSION_T *pSession)
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

OPERATE_RET p2p_send_rtp_data(INT_T client, INT_T channel, CHAR_T *buff, INT_T length)
{
    if (channel < TUYA_VDATA_CHANNEL || channel > TUYA_ADATA_CHANNEL) {
        PR_ERR("input errorclient[%d]channel[%d]", client, channel);
        return OPRT_INVALID_PARM;
    }
    INT_T ret = 0;
    // Send data
    if ((0 == (P2P_VIDEO & sg_p2p_session->cmd)) && (0 == (P2P_PB_VIDEO & sg_p2p_session->cmd)) &&
        (0 == (P2P_AUDIO & sg_p2p_session->cmd)) && (0 == (P2P_PB_AUDIO & sg_p2p_session->cmd))) {
        return OPRT_OK;
    }
    ret = tuya_p2p_rtc_send_data(sg_p2p_session->session, channel, buff, length, -1);
    if (ret != length) {
        PR_ERR("Write data failed [%d][%d]", ret, length);
    }
    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_ext_protocol_pack
 *  Note:Transport extension protocol packet assembly
 *  Input: client channel number, pResult result buffer, type 0/1 video/audio
 *  Output: pResultLen result buffer size
 *  Return:
 ***********************************************************/
STATIC VOID __p2p_ext_protocol_pack(INT_T client, INT_T type, CHAR_T *p_result, INT_T *p_result_len)
{
    if (NULL == p_result || NULL == p_result_len) {
        PR_ERR("input error");
        return;
    }

    INT_T fix_len = 0; // 20180428 supplementary header data
    UINT64_T tmpTime;
    INT_T ipcChan = client;
    IPC_STREAM_E curClirtyChn = p2p_get_chn_idx(sg_p2p_session->cur_clarity);
    C2C_AV_TRANS_FIXED_HEADER *pav_Info = (C2C_AV_TRANS_FIXED_HEADER *)p_result;

    if (0 == type) {
        tmpTime = sg_p2p_session->v_timestamp;
        pav_Info->request_id = sg_p2p_session->video_req_id;
        if (TRUE == sg_p2p_session->key_frame) {
            fix_len = sizeof(C2C_AV_TRANS_FIXED_HEADER) + EXT_PROTOCOL_V0_LEN;
            pav_Info->extension_length = 8;
            *(BYTE_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER)] = TY_EXT_VIDEO_PARAM;
            *(BYTE_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 1] = 0;
            *(SHORT_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 2] =
                (SHORT_T)sg_p2p_session->av_Info.width[curClirtyChn];
            *(SHORT_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 4] =
                (SHORT_T)sg_p2p_session->av_Info.height[curClirtyChn];
            *(SHORT_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 6] =
                (SHORT_T)sg_p2p_session->av_Info.fps[curClirtyChn];
        } else {
            fix_len = sizeof(C2C_AV_TRANS_FIXED_HEADER) + 4;
            pav_Info->extension_length = 0;
        }
    } else {
        tmpTime = sg_p2p_session->a_timestamp;
        pav_Info->request_id = sg_p2p_session->audio_req_id;
        fix_len = sizeof(C2C_AV_TRANS_FIXED_HEADER) + EXT_PROTOCOL_V0_LEN;
        pav_Info->extension_length = 8;
        *(BYTE_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER)] = TY_EXT_AUDIO_PARAM;
        *(BYTE_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 1] = 0;
        *(SHORT_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 2] = (SHORT_T)sg_p2p_session->av_Info.audio_sample;
        *(SHORT_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 4] = (SHORT_T)sg_p2p_session->av_Info.audio_channel;
        *(SHORT_T *)&p_result[sizeof(C2C_AV_TRANS_FIXED_HEADER) + 6] = (SHORT_T)sg_p2p_session->av_Info.audio_databits;
    }
    pav_Info->time_ms = tmpTime;
    *p_result_len = fix_len;

    return;
}

STATIC OPERATE_RET __p2p_check_free_buffer_size(INT_T client, INT_T channel, INT_T len)
{
    OPERATE_RET ret = OPRT_OK;
    INT_T sendFreeSize = 0;
    INT_T writeSize = 0;

    ret = tuya_p2p_rtc_check_buffer(sg_p2p_session->session, channel, (uint32_t *)&writeSize, NULL,
                                    (uint32_t *)&sendFreeSize);
    if (OPRT_OK != ret) {
        return ret;
    }

    INT_T rtp_cnt = len / RTP_MTU_LEN + 1;
    INT_T need_size = rtp_cnt * 1600; // kcp send, one segment occupies 1600 bytes
    if (need_size > sendFreeSize) {
        STATIC INT_T retry_sum = 0; // Total retry count when buffer is full
        if (retry_sum % 100 == 0) {
            PR_ERR("Check_Buffer not enough writeSize[%d] sendFreeSize[%d] len[%d] session[%d] channel[%d]", writeSize,
                   sendFreeSize, len, sg_p2p_session->session, channel);
        }
        retry_sum++;
        ret = OPRT_RESOURCE_NOT_READY;
    }
    return ret;
}

/***********************************************************
 *  Function: __p2p_pack_h265_rtp_and_send
 *  Note:IPC stream data assembly RTP and send
 *  Input: pData data header address, len data length, client channel number
 *  Output: none
 *  Return:
 ***********************************************************/
STATIC OPERATE_RET __p2p_pack_h265_rtp_and_send(INT_T client, CHAR_T *pData, INT_T len)
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
    uint32_t timestamp = (UINT_T)sg_p2p_session->v_pts;
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
STATIC OPERATE_RET __p2p_pack_h264_rtp_and_send(INT_T client, CHAR_T *pData, INT_T len)
{
    if (NULL == pData) {
        PR_ERR("input error");
        return OPRT_INVALID_PARM;
    }

    UINT_T max_frame_size = /*tuya_ipc_media_adapter_get_max_frame(0, 0, 0)*/ (300 * 1024);
    if (len > max_frame_size) {
        PR_ERR("frame len too big[%d]", len);
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
    uint32_t timestamp = (UINT_T)sg_p2p_session->v_pts;
    pRtpDelegate = rtp_payload_encode_create(/*H264_PAY_LOAD*/ 96, "H264", seq, ssrc, &rtp_packer, &rtp_pack_nal_arg);
    ret = rtp_payload_encode_input(pRtpDelegate, pData, len, timestamp);
    if (OPRT_OK != ret) {
        PR_ERR("rtp_payload_encode_input h264 error:%d", ret);
    }
    rtp_payload_encode_getinfo(pRtpDelegate, &sg_p2p_session->video_seq_num, &timestamp);
    rtp_payload_encode_destroy(pRtpDelegate);

    return ret;
}

/***********************************************************
 *  Function: __p2p_pack_aac_rtp_and_send
 *  Note:IPC stream data assembly RTP and send
 *  Input: pData data header address, len data length, client channel number
 *  Output: none
 *  Return:
 ***********************************************************/
// STATIC OPERATE_RET __p2p_pack_aac_rtp_and_send(INT_T client, CHAR_T *pData, INT_T len)
// {
//     if (NULL == pData) {
//         PR_ERR("data[%p] client num [%d]",pData, client);
//         return OPRT_INVALID_PARM;
//     }
//     //Process according to 1-n frames
//     INT_T i;
//     OPERATE_RET ret = OPRT_OK;
//     ADTS_HEADER strAdts = {0};
//     INT_T audioRtpLen = 0;

//     PR_DEBUG("aac audio len[%d]",len);

// 	ret = __p2p_check_free_buffer_size(client,TUYA_ADATA_CHANNEL,len);
// 	if (OPRT_OK != ret) {
//         return ret;
//     }

//     if (NULL == sg_p2p_session->p_audio_rtp_buff) {
//         PR_ERR("audio rtp buffer is NULL");
//         return OPRT_INVALID_PARM;
//     }

//     INT_T fix_len = 0; //20180428 Added header data
//     CHAR_T ext_head_buff[P2P_EXT_HEAD_MAX_LEN] = {0};    //Based on extended video header protocol
//     head+ext(8)+rtp_len

//     __p2p_ext_protocol_pack(client, 1, ext_head_buff, &fix_len);

//     for (i = 0; i < len;) {
//         //ADTS header parsing
//         if (OPRT_OK != tuya_ipc_parse_adts_header((UCHAR_T * )&pData[i], &strAdts)) {
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
//             if (OPRT_OK == tuya_ipc_pack_aac_rtp((BYTE_T * )(pData + i + ADTS_HEADER_LENGTH),
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
STATIC OPERATE_RET __p2p_pack_g711_rtp_and_send(INT_T client, CHAR_T *pData, INT_T len, INT_T mode)
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
    uint32_t timestamp = (UINT_T)sg_p2p_session->a_pts;
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

OPERATE_RET tuya_ipc_delete_video_finish_v2(IN CONST UINT_T client, TUYA_DOWNLOAD_DATA_TYPE type, int success)
{
    return OPRT_OK;
}

OPERATE_RET tuya_ipc_p2p_set_limit_mode(BOOL_T islimit)
{
    return OPRT_OK;
}

OPERATE_RET tuya_ipc_init_trans_av_info(TRANS_IPC_AV_INFO_T *av_info)
{
    memcpy(&sg_p2p_session->av_Info, av_info, sizeof(TRANS_IPC_AV_INFO_T));
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
STATIC INT_T __p2p_session_trans_video_start(P2P_SESSION_T *pSession)
{
    if (NULL == pSession || (P2P_VIDEO & pSession->cmd)) {
        PR_ERR("param error or video started");
        return OPRT_INVALID_PARM;
    }
    // Wait for previous data transmission to end
    PR_DEBUG("session[%d]video video_start wait_concurr_idle", pSession->session);
    pSession->cmd |= P2P_VIDEO;
    PR_DEBUG("session[%d] video start success", pSession->session);
    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_session_trans_stop
 *  Note:Close transmission
 *  Input:pSession Session management
 *  Output: none
 *  Return:
 ***********************************************************/
STATIC INT_T __p2p_session_trans_video_stop(P2P_SESSION_T *pSession)
{
    if (NULL == pSession || !(P2P_VIDEO & pSession->cmd)) {
        PR_ERR("param error or session cmd[%d]", pSession->cmd);
        return OPRT_INVALID_PARM;
    }
    pSession->cmd &= ~P2P_VIDEO;
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
STATIC INT_T __p2p_session_trans_audio_start(P2P_SESSION_T *pSession)
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
STATIC INT_T __p2p_session_trans_audio_stop(P2P_SESSION_T *pSession)
{
    if (NULL == pSession || !(P2P_AUDIO & pSession->cmd)) {
        PR_ERR("param error or audio not start");
        return OPRT_INVALID_PARM;
    }
    pSession->cmd &= ~P2P_AUDIO;
    PR_DEBUG("session:[%d] audio stop success", pSession->session);
    return OPRT_OK;
}

/***********************************************************
 *  Function: __p2p_session_pack_resp
 *  Note:Response to app query
 *  Input:pSrc  Received data, pPayLoad Queried payload data
 *  Output: none
 *  Return:
 ***********************************************************/
STATIC INT_T __p2p_session_pack_resp(P2P_SESSION_T *pSession, IN VOID *pSrc, IN VOID *pPayLoad, INT_T len)
{
    CHAR_T *sendBuff = NULL;
    INT_T packLen = 0;
    INT_T ret = 0;

    if (NULL == pSrc || NULL == pPayLoad || NULL == pSession) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    packLen = P2P_CMD_HEAD_LEN + len;
    sendBuff = (CHAR_T *)Malloc(packLen);
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

STATIC INT_T __p2p_session_cmd_parse_server(P2P_SESSION_T *pSession, VOID *pData)
{
    P2P_CMD_PARSE_T *pCmd = NULL;
    C2C_CMD_FIXED_HEADER_T *pFixedHead = NULL;
    CHAR_T *pPayload = NULL;

    if (NULL == pSession || NULL == pData) {
        PR_ERR("param error");
        return OPRT_INVALID_PARM;
    }

    pPayload = pData + P2P_CMD_HEAD_LEN;
    pCmd = (P2P_CMD_PARSE_T *)pData;
    pFixedHead = &pCmd->str_header;

    switch (pFixedHead->high_cmd) {
    case TY_C2C_CMD_QUERY_AUDIO_PARAMS: {
        // Query audio parameters (reused for app to query audio types needed for intercom)
        // Send query results to client
        //  PR_DEBUG("recv session[%d] query audio params",pSession->session);
        C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E *pAudioResp = NULL;
        C2C_TRANS_QUERY_AUDIO_PARAM_REQ_T *pAudioReq;
        pAudioReq = (C2C_TRANS_QUERY_AUDIO_PARAM_REQ_T *)pPayload;
        INT_T respLen = sizeof(C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E) + sizeof(AUDIO_PARAM_T);
        pAudioResp = (C2C_TRANS_QUERY_AUDIO_PARAM_RESP_E *)Malloc(respLen);
        if (NULL != pAudioResp) {
            pAudioResp->channel = pAudioReq->channel;
            pAudioResp->count = 1;
            // pAudioResp->audioParams[0].type = sg_p2p_ctl.rev_audio_codec;
            // pAudioResp->audioParams[0].sample_rate = sg_p2p_ctl.audio_sample;
            // pAudioResp->audioParams[0].bitwidth = sg_p2p_ctl.audio_databits;
            // pAudioResp->audioParams[0].channel_num = sg_p2p_ctl.audio_channel;
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
        // Query video parameters
        // Send query results to client
        //  PR_DEBUG("recv session[%d] query video params",pSession->session);
        C2C_TRANS_QUERY_VIDEO_PARAM_RESP_T *pVideoResp = NULL;
        C2C_TRANS_QUERY_VIDEO_PARAM_REQ_T *pVideoReq;
        pVideoReq = (C2C_TRANS_QUERY_VIDEO_PARAM_REQ_T *)pPayload;

        if (NULL != pVideoResp) {
            __p2p_session_pack_resp(pSession, pData, pVideoResp,
                                    sizeof(C2C_TRANS_QUERY_VIDEO_PARAM_RESP_T) +
                                        pVideoResp->count * sizeof(VIDEO_PARAM_T));
            Free(pVideoResp);
        } else {
            // Send failure message to app
            C2C_CMD_IO_CTRL_COM_RESP_T comResp;
            memset(&comResp, 0x00, sizeof(comResp));
            comResp.channel = pVideoReq->channel;
            comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_FAILED;
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
        // Send to device for processing
        C2C_TRANS_LIVE_CLARITY_PARAM_S outParm = {0};
        outParm.clarity =
            (parm->mode == TY_VIDEO_CLARITY_INNER_HIGH) ? TY_VIDEO_CLARITY_HIGH : TY_VIDEO_CLARITY_STANDARD;
        // outParm.clarity = __p2p_clarity_trans(parm->mode);
        PR_DEBUG("set video clarity session[%d]chn[%d] op[%d] clarity[%d]", pSession->session, parm->channel,
                 parm->mode, outParm.clarity);
// tuya_ipc_media_stream_event_call(0, 0, MEDIA_STREAM_LIVE_VIDEO_CLARITY_SET, (VOID *)&outParm);
#if 0
            //Update reqId for app to distinguish different video files
            pSession->video_req_id = pCmd->reqId;
            pSession->cur_clarity = parm->mode;
#endif
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
    default: {
        PR_ERR("CTRL CMD ERROR[%d]", pFixedHead->high_cmd);
        C2C_CMD_IO_CTRL_COM_RESP_T comResp;
        memset(&comResp, 0x00, sizeof(comResp));
        comResp.channel = 0;
        comResp.result = TY_C2C_CMD_IO_CTRL_COMMAND_INVALID;
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
STATIC INT_T __p2p_session_cmd_parse(P2P_SESSION_T *pSession, VOID *pData)
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

STATIC INT_T __p2p_read_cmd(P2P_SESSION_T *pSession)
{
    INT_T ret = 0;
    C2C_CMD_FIXED_HEADER_T *pFixedHeader = NULL;
    P2P_DATA_PARSE_T *pDataParse = &pSession->proto_parse;
    P2P_CMD_PARSE_T *pReadBuff = (P2P_CMD_PARSE_T *)(pDataParse->read_buff);
    ret = tuya_p2p_rtc_recv_data(pSession->session, TUYA_CMD_CHANNEL, pDataParse->read_buff + pDataParse->cur_read,
                                 &pDataParse->read_size, P2P_RECV_TIMEOUT);
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

STATIC void __p2p_cmd_recv_proc(PVOID_T pArg)
{
    P2P_SESSION_T *pSession = NULL;
    INT_T ret;

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
            __p2p_session_clear(pSession);
            //__p2p_wait_concurr_idle(pSession, WAIT_ALL_BUF);
            __p2p_session_release_va(pSession);
            tuya_p2p_rtc_notify_exit();
            printf("pSession->cmd: %d\n", sg_p2p_session->cmd);
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
STATIC void __p2p_media_send_proc(PVOID_T pArg)
{
    INT_T index = 0;
    UINT_T runCnt = 0;
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
        INT_T status = pSession->status;
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

        if (P2P_VIDEO & cmd) {
            if (sg_p2p_session->on_get_video_frame_callback == NULL) {
                tal_system_sleep(10);
                continue;
            }
            MEDIA_FRAME *pMediaFrame = &sg_p2p_session->media_frame;
            op_ret = sg_p2p_session->on_get_video_frame_callback(pMediaFrame); // OnGetVideoFrameCallback(pMediaFrame)
            if (op_ret == OPRT_OK) {
                pSession->v_pts = (pMediaFrame->pts == 0) ? pMediaFrame->timestamp * 1000 : pMediaFrame->pts;
                pSession->v_timestamp = pMediaFrame->timestamp;
                if (eVideoIFrame == pMediaFrame->type) {
                    pSession->key_frame = TRUE;
                } else {
                    pSession->key_frame = FALSE;
                }
                if (TY_AV_CODEC_VIDEO_H265 != sg_p2p_session->av_Info.video_codec[0]) {
                    op_ret = __p2p_pack_h264_rtp_and_send(index, (CHAR_T *)pMediaFrame->data, pMediaFrame->size);
                } else {
                    op_ret = __p2p_pack_h265_rtp_and_send(index, (CHAR_T *)pMediaFrame->data, pMediaFrame->size);
                }
            } else {
                // Buffer has no data yet
                tal_system_sleep(10);
            }
        }
        if (P2P_AUDIO & cmd) {
            if (sg_p2p_session->on_get_audio_frame_callback == NULL) {
                tal_system_sleep(10);
                continue;
            }
            MEDIA_FRAME *pMediaFrame = &sg_p2p_session->media_audio_frame;
            op_ret = sg_p2p_session->on_get_audio_frame_callback(pMediaFrame); // OnGetAudioFrameCallback(pMediaFrame)
            if (op_ret == OPRT_OK) {
                pSession->a_pts = (pMediaFrame->pts == 0) ? pMediaFrame->timestamp * 1000 : pMediaFrame->pts;
                pSession->a_timestamp = pMediaFrame->timestamp;
                if (TY_AV_CODEC_AUDIO_AAC_ADTS == type) {
                    // op_ret = __p2p_pack_aac_rtp_and_send((CHAR_T *)node_a.data, node_a.size,index);
                } else if (TY_AV_CODEC_AUDIO_G711A == type || TY_AV_CODEC_AUDIO_G711U == type ||
                           TY_AV_CODEC_AUDIO_PCM == type) {
                    op_ret = __p2p_pack_g711_rtp_and_send(index, (CHAR_T *)pMediaFrame->data, pMediaFrame->size, type);
                }
            } else {
                // Buffer has no data yet
                tal_system_sleep(10);
            }
        }
    } // while

    PR_ERR("video send task exit");
    return;
}

INT_T __p2p_session_clear(P2P_SESSION_T *pSession)
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
INT_T __p2p_session_all_stop(P2P_SESSION_T *pSession)
{
    tal_mutex_lock(pSession->cmutex);
    if (NULL == pSession) {
        PR_ERR("param error");
        tal_mutex_unlock(pSession->cmutex);
        return OPRT_INVALID_PARM;
    }
    if (P2P_VIDEO & pSession->cmd) {
        pSession->cmd &= ~P2P_VIDEO;
    }
    if (P2P_AUDIO & pSession->cmd) {
        pSession->cmd &= ~P2P_AUDIO;
    }
    if ((P2P_PB_VIDEO & pSession->cmd) || (P2P_PB_PAUSE & pSession->cmd)) {
        pSession->cmd &= ~P2P_PB_VIDEO;
    }
    tal_mutex_unlock(pSession->cmutex);
    return OPRT_OK;
}

INT_T __p2p_session_release_va(P2P_SESSION_T *pSession)
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
    // if (pSession->media_frame.data != NULL) {
    //     free(pSession->media_frame.data);
    //     pSession->media_frame.data = NULL;
    // }
    // memset(&pSession->media_frame, 0, sizeof(pSession->media_frame));
    // if (pSession->media_audio_frame.data != NULL) {
    //     free(pSession->media_audio_frame.data);
    //     pSession->media_audio_frame.data = NULL;
    // }
    // memset(&pSession->media_audio_frame, 0, sizeof(pSession->media_audio_frame));
    memset(&pSession->proto_parse, 0, sizeof(pSession->proto_parse));
    memset(&pSession->av_Info, 0, sizeof(pSession->av_Info));
    if (pSession->on_disconnect_callback)
        pSession->on_disconnect_callback(); // Notify upper layer when receiving disconnect signal from cloud
    tal_mutex_unlock(pSession->cmutex);
    return 0;
}

OPERATE_RET p2p_init(IN CONST TUYA_IPC_P2P_VAR_T *p_var)
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
    THREAD_CFG_T thrd_param = {STACK_SIZE_P2P_MEDIA_RECV, THREAD_PRIO_2, NULL};
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
    sg_p2p_session->media_frame.data = (UCHAR_T *)malloc(bufSize);
    sg_p2p_session->media_frame.size = bufSize;

    bufSize = 1280;
    // memset(&sg_p2p_session->tal_audio_frame, 0, sizeof(sg_p2p_session->tal_audio_frame));
    // sg_p2p_session->tal_audio_frame.pbuf = (char*)malloc(bufSize);
    // sg_p2p_session->tal_audio_frame.buf_size = bufSize;

    memset(&sg_p2p_session->media_audio_frame, 0, sizeof(sg_p2p_session->media_audio_frame));
    sg_p2p_session->media_audio_frame.data = (UCHAR_T *)malloc(bufSize);
    sg_p2p_session->media_audio_frame.size = bufSize;

    memcpy(&sg_p2p_session->av_Info, &p_var->av_info, sizeof(TRANS_IPC_AV_INFO_T));
    sg_p2p_session->on_disconnect_callback = p_var->on_disconnect_callback;
    sg_p2p_session->on_get_video_frame_callback = p_var->on_get_video_frame_callback;
    sg_p2p_session->on_get_audio_frame_callback = p_var->on_get_audio_frame_callback;

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

OPERATE_RET tuya_imm_p2p_init(IN CONST TUYA_IPC_P2P_VAR_T *p_var)
{
    return p2p_init(p_var);
}

OPERATE_RET tuya_imm_p2p_all_stream_close(INT_T close_reason)
{
    // return tuya_ipc_p2p_stream_close(close_reason);
    return 0;
}

OPERATE_RET tuya_imm_p2p_close(VOID)
{
    // return tuya_ipc_tranfser_close();
    return 0;
}

OPERATE_RET tuya_imm_p2p_alive_cnt()
{
    // return tuya_ipc_p2p_alive_cnt();
    return 0;
}

OPERATE_RET tuya_imm_p2p_delete_video_finish(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                             TUYA_DOWNLOAD_DATA_TYPE type, int success)
{
    // return tuya_ipc_delete_video_finish_v2(client, type, success);
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_download_status(IN CONST CHAR_T *dev_id, IN CONST UINT_T client, IN CONST UINT_T percent)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_download_is_send_over(IN CONST CHAR_T *dev_id, IN CONST UINT_T client)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_download_data(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                           TUYA_DOWNLOAD_DATA_TYPE type, IN CONST void *pHead, IN CONST CHAR_T *pData)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_app_album_play_send_data(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                  IN CONST TUYA_ALBUM_PLAY_FRAME_T *p_frame)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_playback_send_video_frame(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                   IN CONST MEDIA_VIDEO_FRAME_T *p_video_frame)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_playback_send_audio_frame(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                   IN CONST MEDIA_AUDIO_FRAME_T *p_audio_frame)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_playback_send_fragment_end(IN CONST CHAR_T *dev_id, IN CONST UINT_T client,
                                                    IN CONST PLAYBACK_TIME_S *fgmt)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_playback_send_finish(IN CONST CHAR_T *dev_id, IN CONST UINT_T client)
{
    return 0;
}

OPERATE_RET
tuya_imm_p2p_playback_send_video_frame_with_encrypt(IN CONST UINT_T client, IN UINT_T reqId,
                                                    IN CONST TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_video_frame)
{
    return 0;
}

OPERATE_RET
tuya_imm_p2p_playback_send_audio_frame_with_encrypt(IN CONST UINT_T client, IN UINT_T reqId,
                                                    IN CONST TRANSFER_MEDIA_FRAME_WIHT_ENCRYPT_T *p_audio_frame)
{
    return 0;
}

OPERATE_RET tuya_imm_p2p_album_play_send_finish(IN CONST CHAR_T *dev_id, IN CONST UINT_T client)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *rtp_alloc(void *param, int bytes)
{
    int nBufferSize = bytes;
    unsigned char *pBuffer = (unsigned char *)malloc(nBufferSize);
    memset(pBuffer, 0, nBufferSize);
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
    //return 0;
    CHAR_T *buf = (CHAR_T *)packet;
    INT_T len = bytes;
    RTP_PACK_NAL_ARG_T *nal_arg = (RTP_PACK_NAL_ARG_T *)param;
    memcpy(nal_arg->p_rtp_buff, nal_arg->ext_head_buff, nal_arg->fix_len);
    *(INT_T *)&nal_arg->p_rtp_buff[nal_arg->fix_len - 4] = len;
    memcpy(nal_arg->p_rtp_buff + nal_arg->fix_len, buf, len);
    return p2p_send_rtp_data(nal_arg->client, nal_arg->channel, nal_arg->p_rtp_buff, len + nal_arg->fix_len);
}

////////////////////////////////////////////////////////////////////////////////////////////

INT_T OnGetVideoFrameCallback(MEDIA_FRAME *pMediaFrame)
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

INT_T OnGetAudioFrameCallback(MEDIA_FRAME *pMediaFrame)
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

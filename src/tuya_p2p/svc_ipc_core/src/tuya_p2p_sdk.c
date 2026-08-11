#include "tuya_p2p_sdk.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "tal_log.h"
#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "tuya_lan.h"
#include "tuya_ipc_skill.h"
#include "tuya_media_service_rtc.h"
#include "tuya_ipc_media_stream.h"
#include "tuya_ipc_media_stream_common.h"

#define PRE_TOPIC     "smart/device/in/"
#define MQ_SERV_TOPIC "smart/device/out/"
void tuya_ipc_upload_skills(void);
OPERATE_RET gw_active_set_ext_param(char *param);
char *gw_active_get_ext_param(void);
OPERATE_RET httpc_gw_active(const GW_ACTV_IN_PARM_V41_S *param, cJSON **result);
OPERATE_RET __p2p_v3_login_init(int preconnect, int max_client, int bitrate);
void tuya_p2p_rtc_signaling_cb(char *remote_id, char *signaling, uint32_t len);
void tuya_p2p_lan_signaling_cb(char *remote_id, char *signaling, uint32_t len);
static int ipc_lan_cmd_cb(const uint8_t *data, uint8_t **out);

OPERATE_RET TUYA_APP_Start(TUYA_IPC_SDK_VAR_S *pSdkVar)
{
    OPERATE_RET ret = OPRT_OK;

    // Set activation skill parameters and report
    TUYA_IPC_SKILL_PARAM_U skill_param = {.value = 0};
    skill_param.value = tuya_p2p_rtc_get_skill();
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_P2P, &skill_param);
    skill_param.value = 1;
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_LOWPOWER, &skill_param);
#if defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_LOCALSTG, &skill_param);
#endif
#if defined(ENABLE_IPC_CLOUD_STORE) && (ENABLE_IPC_CLOUD_STORE == 1)
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_CLOUDSTG, &skill_param);
#endif
    tuya_ipc_upload_skills();

    // Initialize P2P component
    MEDIA_STREAM_VAR_T stream_var = {0};
    stream_var.max_client_num = 1;
    stream_var.def_live_mode = TRANS_DEFAULT_STANDARD;
    stream_var.recv_buffer_size = 16 * 1024;
    int preconnect = stream_var.low_power ? 0 : 1;
    /* Align TuyaOS wukong: sdkVar.media_info.video_bitrate[MAIN] = 1M, same
     * value the demo encoder is configured with (DEMO_CAM_BITRATE_KB). */
    ret = __p2p_v3_login_init(preconnect, stream_var.max_client_num, TUYA_VIDEO_BITRATE_1M);
    if (OPRT_OK != ret) {
        PR_ERR("__p2p_v3_login_init failed\n");
        return ret;
    }

    p2p_rtc_listen_start();

    TUYA_IPC_P2P_VAR_T var = {0};
    var.max_client_num = stream_var.max_client_num;
    var.def_live_mode = stream_var.def_live_mode;
    var.low_power = stream_var.low_power;
    var.recv_buffer_size = stream_var.recv_buffer_size;
    var.on_disconnect_callback = pSdkVar->OnSignalDisconnectCallback;
    var.on_get_video_frame_callback = pSdkVar->OnGetVideoFrameCallback;
    var.on_get_audio_frame_callback = pSdkVar->OnGetAudioFrameCallback;
    var.on_live_video_start_callback = pSdkVar->OnLiveVideoStartCallback;
    var.on_live_video_stop_callback = pSdkVar->OnLiveVideoStopCallback;
    var.on_live_audio_start_callback = pSdkVar->OnLiveAudioStartCallback;
    var.on_live_audio_stop_callback = pSdkVar->OnLiveAudioStopCallback;
    var.on_recv_audio_frame_callback = pSdkVar->OnRecvAudioFrameCallback;
    if (var.recv_buffer_size == 0) {
        var.recv_buffer_size = 16 * 1024;
    }
    ret = p2p_init(&var);
    if (OPRT_OK != ret) {
        PR_ERR("tuya_ipc_p2p_init failed \n");
        return ret;
    }
    return ret;
}

OPERATE_RET TUYA_APP_End()
{
    return 0;
}

OPERATE_RET OnIotInited()
{
    OPERATE_RET rt = OPRT_OK;
    //mqtt extra init cb
    //tuya_ipc_mqtt_register_cb_init();
    // Enable skill
    TUYA_IPC_SKILL_PARAM_U skill_param = {.value = 1};
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_LOWPOWER, &skill_param);
    // Set activation skill parameters
    char *ipc_skills = NULL;
#if defined(HARDWARE_INFO_CHECK) && (HARDWARE_INFO_CHECK == 1)
    int len = 4096;
    ipc_skills = (char *)malloc(len);
#else
    int len = 256;
    ipc_skills = (char *)malloc(len);
#endif
    memset(ipc_skills, 0, len);
    if (ipc_skills) {
        // strcpy(ipc_skills, "\"skillParam\":\"");
        snprintf(ipc_skills + strlen(ipc_skills), len - strlen(ipc_skills),
                 "{\\\"type\\\":%d,\\\"skill\\\":", TUYA_P2P); // P2P type
        tuya_ipc_http_fill_skills_cb(ipc_skills);
#if defined(HARDWARE_INFO_CHECK) && (HARDWARE_INFO_CHECK == 1)
        TUYA_IPC_SENSOR_INFO_T sensor_info = {0};
        tuya_ipc_hardware_info_fill(ipc_skills, &sensor_info);
#endif
        // strcat(ipc_skills,"}\"");
        strcat(ipc_skills, "}");
    }
    gw_active_set_ext_param(ipc_skills);

#if defined(ENABLE_IPC_4G) && (ENABLE_IPC_4G == 1)
    tuya_ipc_dev_manager_init();
#endif

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static char *s_ext_param = NULL; // user defined functions
OPERATE_RET gw_active_set_ext_param(char *param)
{
    s_ext_param = param;
    return OPRT_OK;
}

char *gw_active_get_ext_param()
{
    return s_ext_param;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void gw_p2p_mqtt_data_cb(cJSON *root_json)
{
    int ret = 0;
    uint32_t msg_len = 0;
    if (root_json == NULL) {
        PR_ERR("root_json is null");
        return;
    }

    // cJSON *json = NULL;
    // json = cJSON_GetObjectItem(root_json, "data");
    // if (NULL == json){
    //     PR_ERR("data failed");
    //     return ;
    // }

    char *sendBuff = NULL;
    sendBuff = cJSON_PrintUnformatted(root_json);
    if (NULL == sendBuff) {
        PR_ERR("send buff is NULL");
        return;
    }
    msg_len = (uint32_t)strlen(sendBuff);

    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    ret = tuya_p2p_rtc_set_signaling(NULL, sendBuff, msg_len);
    // ret = tuya_p2p_parse_signal(gw_cntl->gw_if.id, sendBuff, strlen(sendBuff));
    if (OPRT_OK != ret) {
        PR_ERR("tuya_p2p_rtc_set_signaling error ret=%d", ret);
    }

    if (sendBuff) {
        cJSON_free(sendBuff);
    }

    return;
}

OPERATE_RET __p2p_v3_login_init(int preconnect, int max_client, int bitrate)
{
    OPERATE_RET mqttP2pRet = OPRT_OK;

    tuya_iot_client_t *pIotClient = tuya_iot_client_get();
    char *dev_id = pIotClient->activate.devid;

    tuya_p2p_rtc_options_t strOpt;
    memset(&strOpt, 0x00, sizeof(tuya_p2p_rtc_options_t));
    memcpy(strOpt.local_id, /*gw_cntl->gw_if.id*/ dev_id, /*sizeof(gw_cntl->gw_if.id)*/ strlen(dev_id));

    strOpt.preconnect_enable = preconnect;
    strOpt.fragement_len = 1300; /* align TuyaOS mid_p2p dosend fragment */
    // strOpt.cb.on_moto_signaling = tuya_p2p_rtc_moto_signaling_cb;
    strOpt.cb.on_signaling = tuya_p2p_rtc_signaling_cb;
    strOpt.cb.on_lan_signaling = tuya_p2p_lan_signaling_cb;
    // strOpt.cb.on_log            = __media_service_rtc_log_upload;
    // strOpt.cb.on_log_get_level  = tuya_imm_service_log_get_level;
    // strOpt.cb.on_auth           = tuya_p2p_rtc_auth;
    strOpt.max_channel_number = /*TUYA_CHANNEL_MAX*/ 6;
    strOpt.max_session_number = max_client;
    strOpt.max_pre_session_number = max_client;
    /*
     * video_bitrate_kbps sizes the video channel memory, exactly as TuyaOS
     * does from sdkVar.media_info.video_bitrate[]. It used to be passed as 0
     * here with send_buf_size hardcoded to 1.1 MB, which is above the OS
     * maximum and let the queue grow to about eight seconds of video before
     * anything was dropped.
     */
    strOpt.video_bitrate_kbps = bitrate;

    uint32_t vsend = (bitrate * 1024u / 8u) * TUYA_P2P_SEND_BUFFER_SECONDS;
    if (vsend > TUYA_P2P_SEND_BUFFER_SIZE_MAX) {
        vsend = TUYA_P2P_SEND_BUFFER_SIZE_MAX;
    } else if (vsend < TUYA_P2P_SEND_BUFFER_SIZE_MIN) {
        vsend = TUYA_P2P_SEND_BUFFER_SIZE_MIN;
    }
    PR_NOTICE("p2p video send buffer %u bytes for %d kbps", vsend, bitrate);

    strOpt.send_buf_size[TUYA_CMD_CHANNEL] = 4096;
    strOpt.recv_buf_size[TUYA_CMD_CHANNEL] = 4096;
    strOpt.send_buf_size[TUYA_VDATA_CHANNEL] = vsend;
    strOpt.recv_buf_size[TUYA_VDATA_CHANNEL] = 1024;
    strOpt.send_buf_size[TUYA_ADATA_CHANNEL] = 2 * P2P_WR_BF_MAX_SIZE + P2P_SEND_REDUNDANCE_LEN;
    strOpt.recv_buf_size[TUYA_ADATA_CHANNEL] = 1024 * 64;
    strOpt.send_buf_size[TUYA_TRANS_CHANNEL] = P2P_WR_BF_MAX_SIZE + P2P_SEND_REDUNDANCE_LEN;
    strOpt.recv_buf_size[TUYA_TRANS_CHANNEL] = 1024;
    strOpt.send_buf_size[TUYA_TRANS_CHANNEL5] = P2P_WR_BF_MAX_SIZE + P2P_SEND_REDUNDANCE_LEN;
    strOpt.recv_buf_size[TUYA_TRANS_CHANNEL5] = 1024 * 1024; // do not use
    mqttP2pRet = tuya_p2p_rtc_init(&strOpt);
    if (0 != mqttP2pRet) {
        PR_ERR("mqtt p2p init failed");
        return -2;
    }
    /* Align TuyaOS media_stream: App LAN retry/signaling uses frame type 0x20 */
    mqttP2pRet = tuya_lan_register_cb(FRM_LAN_P2P_SIGNAL, ipc_lan_cmd_cb);
    if (OPRT_OK != mqttP2pRet) {
        PR_ERR("tuya_lan_register_cb(0x20) failed:%d", mqttP2pRet);
        return mqttP2pRet;
    }
    return mqttP2pRet;
}

/**
 * @brief LAN Type=0x20 P2P signaling handler (App retry / LAN path)
 * @param[in] data decrypted LAN payload (NUL-terminated JSON)
 * @param[out] out optional reply body (unused, keep NULL)
 * @return OPRT_OK on success
 * @note Align OS: only enqueue via tuya_p2p_rtc_set_signaling (worker does ICE).
 */
static int ipc_lan_cmd_cb(const uint8_t *data, uint8_t **out)
{
    size_t len;

    if (out != NULL) {
        *out = NULL;
    }
    if (data == NULL) {
        PR_ERR("ipc_lan_cmd_cb get null data");
        return OPRT_INVALID_PARM;
    }
    len = strlen((const char *)data);
    if (len == 0) {
        return OPRT_INVALID_PARM;
    }
    if (tuya_p2p_rtc_set_signaling(NULL, (char *)data, (uint32_t)len) != 0) {
        PR_ERR("tuya_p2p_rtc_set_signaling error");
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @brief Send P2P answer/candidate signaling back to App over LAN Type=0x20
 * @param[in] remote_id peer id (unused, keep for cb signature)
 * @param[in] signaling JSON signaling body
 * @param[in] len signaling length in bytes
 * @return none
 */
void tuya_p2p_lan_signaling_cb(char *remote_id, char *signaling, uint32_t len)
{
    (void)remote_id;
    if ((signaling == NULL) || (len == 0)) {
        PR_ERR("lan signaling invalid");
        return;
    }
    PR_DEBUG("lan signaling report len:%u", len);
    (void)tuya_lan_data_report(FRM_LAN_P2P_SIGNAL, 0, (uint8_t *)signaling, len);
}

void tuya_p2p_rtc_signaling_cb(char *remote_id, char *signaling, uint32_t len)
{
    // Send answer signaling
    tuya_iot_client_t *pIotClient = tuya_iot_client_get();
    char *dev_id = pIotClient->activate.devid;

    char send_topic[18 + GW_ID_LEN] = {0};
    snprintf(send_topic, SIZEOF(send_topic), "%s%s", MQ_SERV_TOPIC, dev_id);
    PR_DEBUG("mqtt send topic:%s", send_topic);
    tuya_mqtt_protocol_data_publish_with_topic(&pIotClient->mqctx, send_topic, PRO_RTC_REQ, (const uint8_t *)signaling,
                                               (uint16_t)len);

    return;
}
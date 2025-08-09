#include "tuya_p2p_sdk.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "tal_log.h"
#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "tuya_ipc_skill.h"
#include "tuya_media_service_rtc.h"
#include "tuya_ipc_media_stream.h"
#include "tuya_ipc_media_stream_common.h"

#define PRE_TOPIC     "smart/device/in/"
#define MQ_SERV_TOPIC "smart/device/out/"

VOID tuya_ipc_upload_skills(VOID);
OPERATE_RET gw_active_set_ext_param(IN CHAR_T *param);
CHAR_T *gw_active_get_ext_param(VOID);
OPERATE_RET httpc_gw_active(IN CONST GW_ACTV_IN_PARM_V41_S *param, OUT cJSON **result);
OPERATE_RET __p2p_v3_login_init(INT_T preconnect, INT_T max_client, INT_T bitrate);
VOID tuya_p2p_rtc_signaling_cb(CHAR_T *remote_id, CHAR_T *signaling, UINT_T len);

OPERATE_RET TUYA_APP_Start(TUYA_IPC_SDK_VAR_S *pSdkVar)
{
    OPERATE_RET ret = OPRT_OK;

    // Set activation skill parameters and report
    TUYA_IPC_SKILL_PARAM_U skill_param = {.value = 0};
    skill_param.value = tuya_p2p_rtc_get_skill();
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_P2P, &skill_param);
    skill_param.value = 1;
    tuya_ipc_skill_enable(TUYA_IPC_SKILL_LOWPOWER, &skill_param);
    tuya_ipc_upload_skills();

    // Initialize P2P component
    MEDIA_STREAM_VAR_T stream_var = {0};
    stream_var.max_client_num = 1;
    stream_var.def_live_mode = TRANS_DEFAULT_STANDARD;
    stream_var.recv_buffer_size = 16 * 1024;
    INT_T preconnect = stream_var.low_power ? 0 : 1;
    ret = __p2p_v3_login_init(preconnect, stream_var.max_client_num, /*media_info.av_encode_info.video_bitrate[0]*/ 0);
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
    CHAR_T *ipc_skills = NULL;
#if defined(HARDWARE_INFO_CHECK) && (HARDWARE_INFO_CHECK == 1)
    int len = 4096;
    ipc_skills = (CHAR_T *)malloc(len);
#else
    int len = 256;
    ipc_skills = (CHAR_T *)malloc(len);
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

STATIC CHAR_T *s_ext_param = NULL; // user defined functions
OPERATE_RET gw_active_set_ext_param(IN CHAR_T *param)
{
    s_ext_param = param;
    return OPRT_OK;
}

CHAR_T *gw_active_get_ext_param()
{
    return s_ext_param;
}

///////////////////////////////////////////////////////////////////////////////////////////////

VOID gw_p2p_mqtt_data_cb(IN cJSON *root_json)
{
    int ret = 0;
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

    CHAR_T *sendBuff = NULL;
    sendBuff = cJSON_PrintUnformatted(root_json);
    if (NULL == sendBuff) {
        PR_ERR("send buff is NULL");
        return;
    }

    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    ret = tuya_p2p_rtc_set_signaling(NULL, sendBuff, strlen(sendBuff));
    // ret = tuya_p2p_parse_signal(gw_cntl->gw_if.id, sendBuff, strlen(sendBuff));
    if (OPRT_OK != ret) {
        PR_ERR("tuya_p2p_rtc_set_signaling error");
    }

    if (sendBuff) {
        cJSON_free(sendBuff);
    }

    return;
}

OPERATE_RET __p2p_v3_login_init(INT_T preconnect, INT_T max_client, INT_T bitrate)
{
    OPERATE_RET mqttP2pRet = OPRT_OK;

    tuya_iot_client_t *pIotClient = tuya_iot_client_get();
    char *dev_id = pIotClient->activate.devid;

    tuya_p2p_rtc_options_t strOpt;
    memset(&strOpt, 0x00, sizeof(tuya_p2p_rtc_options_t));
    memcpy(strOpt.local_id, /*gw_cntl->gw_if.id*/ dev_id, /*sizeof(gw_cntl->gw_if.id)*/ strlen(dev_id));

    strOpt.preconnect_enable = preconnect;
    strOpt.fragement_len = /*RTP_MTU_LEN*/ 1100 + 100; // Reserve 100 bytes for RTP header and private header
    // strOpt.cb.on_moto_signaling = tuya_p2p_rtc_moto_signaling_cb;
    strOpt.cb.on_signaling = tuya_p2p_rtc_signaling_cb;
    // strOpt.cb.on_lan_signaling  = tuya_p2p_lan_signaling_cb;
    // strOpt.cb.on_log            = __media_service_rtc_log_upload;
    // strOpt.cb.on_log_get_level  = tuya_imm_service_log_get_level;
    // strOpt.cb.on_auth           = tuya_p2p_rtc_auth;
    strOpt.max_channel_number = /*TUYA_CHANNEL_MAX*/ 6;
    strOpt.max_session_number = max_client;
    strOpt.max_pre_session_number = max_client;
    strOpt.video_bitrate_kbps =
        bitrate; // Current video_bitrate_kbps parameter is used for setting webrtc channel memory size in p2p library
    strOpt.send_buf_size[TUYA_CMD_CHANNEL] = 4096;
    strOpt.recv_buf_size[TUYA_CMD_CHANNEL] = 4096;
    strOpt.send_buf_size[TUYA_VDATA_CHANNEL] = (300 * 1024) * 1.1;
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
    return mqttP2pRet;
}

VOID tuya_p2p_rtc_signaling_cb(CHAR_T *remote_id, CHAR_T *signaling, UINT_T len)
{
    // Send answer signaling
    tuya_iot_client_t *pIotClient = tuya_iot_client_get();
    char *dev_id = pIotClient->activate.devid;

    CHAR_T send_topic[18 + GW_ID_LEN] = {0};
    snprintf(send_topic, SIZEOF(send_topic), "%s%s", MQ_SERV_TOPIC, dev_id);
    PR_DEBUG("mqtt send topic:%s", send_topic);
    tuya_mqtt_protocol_data_publish_with_topic(&pIotClient->mqctx, send_topic, PRO_RTC_REQ, signaling, len);

    return;
}
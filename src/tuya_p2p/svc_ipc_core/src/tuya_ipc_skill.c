#include "tuya_ipc_skill.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "tal_log.h"
#include "tal_time_service.h"
#include "tuya_ipc_media_adapter.h"
#include "atop_service.h"

#define IOT_SDK_VER "6.2.1"

#ifdef __UT_TEST_
#define STATIC
#endif
#if (ENABLE_CLOUD_RULE == 1)
#define MAX_AI_SKILL_NUM 5
STATIC CHAR_T g_ai_skill_table[MAX_AI_SKILL_NUM][32] = {
    "ipc_human", "ipc_cat", "ipc_car", "ipc_face", "ipc_package",
};
#endif
typedef struct {
    INT_T cloud_stg;
    INT_T webrtc;
    INT_T low_power;
    INT_T ai_detect;
    INT_T local_stg;
    INT_T cloud_gw;
    INT_T door_bell_stg;
    INT_T p2p;
    INT_T px;
    CHAR_T *cloud_gw_info;
    CHAR_T *media_info;
    INT_T person_flow;
    INT_T upnp;
    INT_T max_p2p_sessions;
    INT_T cloud_rule;
    INT_T local_ai_skills;
    INT_T mobile_network;
} tuya_ipc_skill_info_s;

STATIC tuya_ipc_skill_info_s g_skill_info = {0};
STATIC BOOL_T sg_uploaded = FALSE;

#define SKILL_PARAM_LEN 1024
// Force HTTPS POST 4.1
#define TI_DEV_SKILL_UPDATE            "tuya.device.skill.update"
#define TI_DEV_SKILL_MULTIMEDIA_UPDATE "tuya.device.skill.multimedia.update"
extern VOID_T *tkl_system_psram_malloc(CONST SIZE_T size);
extern VOID_T tkl_system_psram_free(VOID_T *ptr);
// extern int tuya_ipc_ring_buffer_get_video_num_skill(int,int);

INT_T tuya_imm_get_security_ability(VOID);

OPERATE_RET httpc_dev_update_skill_multimedia(IN CONST CHAR_T *gw_id, IN CONST CHAR_T *skill)
{
    // HTTPC_NULL_CHECK(gw_id);
    // HTTPC_NULL_CHECK(skill);

    INT_T buffer_len = 70 + strlen(skill);
    CHAR_T *post_data = tkl_system_psram_malloc(buffer_len);
    if (post_data == NULL) {
        PR_ERR("tkl_system_psram_malloc Fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(post_data, 0, buffer_len);

    snprintf(post_data, buffer_len, "%s", skill);

    OPERATE_RET op_ret = OPRT_OK;
    // op_ret = iot_httpc_common_post_no_remalloc(TI_DEV_SKILL_MULTIMEDIA_UPDATE, "1.0",
    //                            NULL, gw_id,
    //                            post_data, buffer_len, NULL,
    //                            NULL);
    op_ret = atop_service_comm_post_simple(TI_DEV_SKILL_MULTIMEDIA_UPDATE, "1.0", post_data, NULL, NULL);
    tkl_system_psram_free(post_data);
    return op_ret;
}

OPERATE_RET httpc_dev_update_skill_v10(IN CONST CHAR_T *gw_id, IN CONST CHAR_T *sub_id, IN CONST CHAR_T *skill)
{
    // HTTPC_NULL_CHECK(gw_id);
    // HTTPC_NULL_CHECK(skill);

    INT_T buffer_len = 70 + strlen(skill);
    CHAR_T *post_data = tkl_system_psram_malloc(buffer_len);
    if (post_data == NULL) {
        PR_ERR("tkl_system_psram_malloc Fail");
        return OPRT_MALLOC_FAILED;
    }
    memset(post_data, 0, buffer_len);

    if (sub_id == NULL) {
        TIME_T timestamp = 0;
        timestamp = tal_time_get_posix();
        snprintf(post_data, buffer_len, "{\"skill\":\"%s\",\"t\":\"%d\"}", skill,
                 timestamp); // Note: tuya-open needs to add a timestamp here
    } else {
        snprintf(post_data, buffer_len, "{\"subId\":\"%s\",\"skill\":\"%s\"}", sub_id, skill);
    }
    printf("post_data: %s\n", post_data);

    OPERATE_RET op_ret = OPRT_OK;
    // op_ret = iot_httpc_common_post_no_remalloc(TI_DEV_SKILL_UPDATE, "1.0",
    //                            NULL, gw_id,
    //                            post_data, buffer_len, NULL,
    //                            NULL);
    op_ret = atop_service_comm_post_simple(TI_DEV_SKILL_UPDATE, "1.0", post_data, NULL, NULL);
    tkl_system_psram_free(post_data);
    return op_ret;
}

OPERATE_RET http_device_update_skill(IN CONST CHAR_T *dev_id, IN CONST CHAR_T *skill)
{
    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = httpc_dev_update_skill_v10(/*gw_cntl->gw_if.id*/ NULL, dev_id, skill);
    return op_ret;
}

OPERATE_RET http_device_update_skill_multimedia(IN CONST CHAR_T *dev_id, IN CONST CHAR_T *skill)
{
    // GW_CNTL_S *gw_cntl = get_gw_cntl();
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = httpc_dev_update_skill_multimedia(/*gw_cntl->gw_if.id*/ NULL, skill);
    return op_ret;
}

STATIC OPERATE_RET __fill_skills(CHAR_T *skill_info, INT_T skill_len, CHAR_T *skills_buf, INT_T *buf_len)
{
    if (skill_len + *buf_len >= SKILL_PARAM_LEN) {
        PR_ERR("skill buf is not enough\n");
        return OPRT_INVALID_PARM;
    }
    memcpy(skills_buf + *buf_len, skill_info, skill_len);
    *buf_len += skill_len;

    return OPRT_OK;
}

OPERATE_RET tuya_ipc_skill_enable_person_flow(VOID)
{
    TUYA_IPC_SKILL_PARAM_U param = {0};
    param.value = 1;
    return tuya_ipc_skill_enable(TUYA_IPC_SKILL_PERSON_FLOW, &param);
}

VOID tuya_ipc_upload_skills()
{
    CHAR_T *skill_buf = (CHAR_T *)tkl_system_psram_malloc(SKILL_PARAM_LEN);
    if (skill_buf == NULL) {
        return;
    }
    skill_buf[0] = '{';
    INT_T buf_len = 1;

    CHAR_T tmp_buf[128] = {0};
    int len = 0;
    sg_uploaded = TRUE;

#if defined(ENABLE_TMM_LINK) && (ENABLE_TMM_LINK == 1)
    len = sprintf(tmp_buf, "\\\"v\\\":2,\\\"sdk_version\\\":\\\"" IOT_SDK_VER "\\\"");
#else
    len = sprintf(tmp_buf, "\\\"sdk_version\\\":\\\"" IOT_SDK_VER "\\\"");
#endif
    if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
        goto out;
    }

#ifdef IPC_CHANNEL_NUM
    len = sprintf(tmp_buf, ",\\\"channel_num\\\":%d", IPC_CHANNEL_NUM);
    if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
        goto out;
    }
#endif

    if (0 != g_skill_info.ai_detect) {
        len = sprintf(tmp_buf, ",\\\"aiDetect\\\":%d", g_skill_info.ai_detect);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.cloud_gw) {
        len = sprintf(tmp_buf, ",\\\"cloudGW\\\":%d", g_skill_info.cloud_gw);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.cloud_stg) {
        len = sprintf(tmp_buf, ",\\\"cloudStorage\\\":%d", g_skill_info.cloud_stg);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.door_bell_stg) {
        len = sprintf(tmp_buf, ",\\\"doorbellStorage\\\":%d", g_skill_info.door_bell_stg);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.local_stg) {
        len = sprintf(tmp_buf, ",\\\"localStorage\\\":%d", g_skill_info.local_stg);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.low_power) {
        len = sprintf(tmp_buf, ",\\\"lowPower\\\":%d", g_skill_info.low_power);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.webrtc) {
        len = sprintf(tmp_buf, ",\\\"webrtc\\\":%d", g_skill_info.webrtc);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.p2p) {
        len = sprintf(tmp_buf, ",\\\"p2p\\\":%d", g_skill_info.p2p);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.px) {
        len = sprintf(tmp_buf, ",\\\"px\\\":%d", g_skill_info.px);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.upnp) {
        len = sprintf(tmp_buf, ",\\\"upnp\\\":%d", g_skill_info.upnp);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.max_p2p_sessions) {
        len = sprintf(tmp_buf, ",\\\"max_p2p_sessions\\\":%d", g_skill_info.max_p2p_sessions);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    if (0 != g_skill_info.mobile_network) {
        len = sprintf(tmp_buf, ",\\\"mobileNetwork\\\":%d", g_skill_info.mobile_network);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    // int Num = tuya_ipc_ring_buffer_get_video_num_skill(0,0);
    int Num = 1;
    len = sprintf(tmp_buf, ",\\\"video_num\\\":%d", Num);
    if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
        goto out;
    }

    if (g_skill_info.cloud_gw_info) {
        len = strlen(g_skill_info.cloud_gw_info);
        if (len > 0) {
            if (OPRT_OK != __fill_skills(",", 1, skill_buf, &buf_len)) {
                goto out;
            }
            if (OPRT_OK != __fill_skills(g_skill_info.cloud_gw_info, len, skill_buf, &buf_len)) {

                goto out;
            }
        }
    }

    if (0 != g_skill_info.person_flow) {
        len = sprintf(tmp_buf, ",\\\"person_flow\\\":%d", g_skill_info.person_flow);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    INT_T security_ability = tuya_imm_get_security_ability();
    if (security_ability) {
        len = sprintf(tmp_buf, ",\\\"security_ability\\\":%d", security_ability);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

#if (ENABLE_CLOUD_RULE == 1)
    if (0 != g_skill_info.cloud_rule) {
        len = sprintf(tmp_buf, ",\\\"cloudRule\\\":%d", g_skill_info.cloud_rule);
        if (OPRT_OK != __fill_skills(tmp_buf, len, skill_buf, &buf_len)) {
            goto out;
        }
    }

    INT_T i = 0;
    INT_T offset = 0;
    BOOL_T need_comma = FALSE;
    if (0 != g_skill_info.local_ai_skills) {
        len = sprintf(tmp_buf, ",\\\"extSkill\\\":\\\"{");
        offset += len;
        for (i = 0; i < MAX_AI_SKILL_NUM; i++) {
            if (g_skill_info.local_ai_skills & (1 << i)) {
                if (need_comma) {
                    tmp_buf[offset] = ',';
                    offset += 1;
                }
                len = sprintf(tmp_buf + offset, "\\\"%s\\\":true", g_ai_skill_table[i]);
                offset += len;
                need_comma = TRUE;
            }
        }
        len = sprintf(tmp_buf + offset, "}\\\"");
        offset += len;
        if (OPRT_OK != __fill_skills(tmp_buf, offset, skill_buf, &buf_len)) {
            goto out;
        }
    }
#endif
    skill_buf[buf_len] = '}';
    http_device_update_skill(NULL, skill_buf);

#if defined(ENABLE_TMM_LINK) && (ENABLE_TMM_LINK == 1)
    PR_DEBUG("upload data: %s", skill_buf);
    tuya_ipc_upload_media_info();
#endif

out:
    if (skill_buf) {
        tkl_system_psram_free(skill_buf);
        skill_buf = NULL;
    }
    if (g_skill_info.cloud_gw_info) {
        tkl_system_psram_free(g_skill_info.cloud_gw_info);
        g_skill_info.cloud_gw_info = NULL;
    }
    return;
}

OPERATE_RET tuya_ipc_upload_media_info(VOID)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    CHAR_T *skill_buf = tkl_system_psram_malloc(SKILL_PARAM_LEN);
    if (NULL == skill_buf) {
        PR_ERR("malloc skill buf failed");
        return ret;
    }

    if (g_skill_info.media_info) {
        int len = strlen(g_skill_info.media_info);
        if (len > 0) {
            memset(skill_buf, 0, SKILL_PARAM_LEN);
            int buf_len = 0;
            if (OPRT_OK == __fill_skills(g_skill_info.media_info, len, skill_buf, &buf_len)) {
                PR_DEBUG("upload data: %s", skill_buf);
                ret = http_device_update_skill_multimedia(NULL, skill_buf);
            }
        }
        tkl_system_psram_free(g_skill_info.media_info);
        g_skill_info.media_info = NULL;
    }

    if (skill_buf) {
        tkl_system_psram_free(skill_buf);
        skill_buf = NULL;
    }
    PR_DEBUG("upload finish");
    return ret;
}

OPERATE_RET tuya_ipc_skill_enable(IN TUYA_IPC_SKILL_E skill, IN TUYA_IPC_SKILL_PARAM_U *param)
{
    switch (skill) {

    case TUYA_IPC_SKILL_AIDETECT:
        g_skill_info.ai_detect = param->value;
        break;

    case TUYA_IPC_SKILL_CLOUDGW:
        g_skill_info.cloud_gw = g_skill_info.cloud_gw | param->value;
        break;

    case TUYA_IPC_SKILL_CLOUDSTG:
        g_skill_info.cloud_stg = param->value;
        break;

    case TUYA_IPC_SKILL_DOORBELLSTG:
        g_skill_info.door_bell_stg = param->value;
        break;

    case TUYA_IPC_SKILL_LOCALSTG:
        g_skill_info.local_stg = param->value;
        break;

    case TUYA_IPC_SKILL_LOWPOWER:
        g_skill_info.low_power = param->value;
        break;

    case TUYA_IPC_SKILL_WEBRTC:
        g_skill_info.webrtc = param->value;
        break;
    case TUYA_IPC_SKILL_P2P:
        g_skill_info.p2p = param->value;
        break;
    case TUYA_IPC_SKILL_PX:
        g_skill_info.px = param->value;
        break;
    case TUYA_IPC_SKILL_TMM:
        if (g_skill_info.media_info == NULL) {
            g_skill_info.media_info = tkl_system_psram_malloc(SKILL_INFO_LEN);
            if (g_skill_info.media_info == NULL) {
                PR_ERR("malloc media_info failed");
                return OPRT_MALLOC_FAILED;
            }
        }
        strncpy(g_skill_info.media_info, param->info, SKILL_INFO_LEN);
        break;
    case TUYA_IPC_SKILL_PERSON_FLOW:
        g_skill_info.person_flow = param->value;
        break;
    case TUYA_IPC_SKILL_CLOUDGW_INFO:
        if (g_skill_info.cloud_gw_info == NULL) {
            g_skill_info.cloud_gw_info = tkl_system_psram_malloc(SKILL_INFO_LEN);
            if (g_skill_info.cloud_gw_info == NULL) {
                PR_ERR("malloc cloud_gw_info failed");
                return OPRT_MALLOC_FAILED;
            }
        }
        strncpy(g_skill_info.cloud_gw_info, param->info, SKILL_INFO_LEN);
        break;
    case TUYA_IPC_SKILL_UPNP:
        g_skill_info.upnp = param->value;
        break;
    case TUYA_IPC_SKILL_MAX_P2P_SESSIONS:
        g_skill_info.max_p2p_sessions = param->value;
        break;
    case TUYA_IPC_SKILL_CLOUDRULE:
        g_skill_info.cloud_rule = param->value;
        break;
    case TUYA_IPC_SKILL_CLOUDRULE_GW:
        g_skill_info.cloud_gw = g_skill_info.cloud_gw | param->value;
        break;
    case TUYA_IPC_SKILL_MOBILE_NETWORK:
        g_skill_info.mobile_network = param->value;
        break;
    default:
        PR_ERR("unknown skill(%d)\n", skill);
        break;
    }
    return OPRT_OK;
}

VOID tuya_ipc_http_fill_skills_cb(INOUT CHAR_T *skills)
{
    if (NULL == skills) {
        return;
    }
    int Num = 0;
    strcat(skills, "{\\\"localStorage\\\":1");
#if ENABLE_CLOUD_STORAGE == 1
    strcat(skills, ",\\\"cloudStorage\\\":1");
#endif
#if ENABLE_ECHO_SHOW == 1
    strcat(skills, ",\\\"echoshow\\\":2");
#endif
#if ENABLE_CHROMECAST == 1
    strcat(skills, ",\\\"chromecast\\\":2");
#endif

#if ENABLE_MQTT_WEBRTC == 1
    strcat(skills, ",\\\"webrtc\\\":2");
#endif

#if ENABLE_CLOUD_RULE == 1
#if ENABLE_CLOUD_RULE_CRON == 1
    strcat(skills, ",\\\"cloudRule\\\":7");
#else
    strcat(skills, ",\\\"cloudRule\\\":5");
#endif
#endif

    if (g_skill_info.low_power) {
        strcat(skills, ",\\\"lowPower\\\":1");
    }

    INT_T security_ability = tuya_imm_get_security_ability();
    if (security_ability) {
        CHAR_T buf[40] = {0};
        snprintf(buf, SIZEOF(buf), ",\\\"security_ability\\\":%d", security_ability);
        strcat(skills, buf);
    }

    Num = 1 /*tuya_ipc_ring_buffer_get_video_num_skill(0,0)*/;
    unsigned char Data[20] = {0};
    sprintf((char *)Data, ",\\\"video_num\\\":%d", Num);
    strcat(skills, (char *)Data);

    strcat(skills, ",\\\"sdk_version\\\":\\\"" IOT_SDK_VER "\\\"}");
}

BOOL_T tuya_ipc_is_low_power(VOID)
{
    return g_skill_info.low_power != 0;
}

OPERATE_RET tuya_ipc_skill_get(TUYA_IPC_SKILL_E skill, INT_T *result)
{
    OPERATE_RET ret = OPRT_OK;
    switch (skill) {

    case TUYA_IPC_SKILL_AIDETECT:
        *result = g_skill_info.ai_detect;
        break;

    case TUYA_IPC_SKILL_CLOUDGW:
        *result = g_skill_info.cloud_gw;
        break;

    case TUYA_IPC_SKILL_CLOUDSTG:
        *result = g_skill_info.cloud_stg;
        break;

    case TUYA_IPC_SKILL_DOORBELLSTG:
        *result = g_skill_info.door_bell_stg;
        break;

    case TUYA_IPC_SKILL_LOCALSTG:
        *result = g_skill_info.local_stg;
        break;

    case TUYA_IPC_SKILL_LOWPOWER:
        *result = g_skill_info.low_power;
        break;

    case TUYA_IPC_SKILL_WEBRTC:
        *result = g_skill_info.webrtc;
        break;
    case TUYA_IPC_SKILL_P2P:
        *result = g_skill_info.p2p;
        break;
    case TUYA_IPC_SKILL_PX:
        *result = g_skill_info.px;
        break;
    case TUYA_IPC_SKILL_PERSON_FLOW:
        *result = g_skill_info.person_flow;
        break;
    case TUYA_IPC_SKILL_UPNP:
        *result = g_skill_info.upnp;
        break;
    case TUYA_IPC_SKILL_MAX_P2P_SESSIONS:
        *result = g_skill_info.max_p2p_sessions;
        break;
    case TUYA_IPC_SKILL_MOBILE_NETWORK:
        *result = g_skill_info.mobile_network;
        break;
    default:
        PR_ERR("unknown skill(%d)\n", skill);
        ret = OPRT_NOT_SUPPORTED;
        break;
    }
    return ret;
}

OPERATE_RET tuya_ipc_cloud_gw_kills_get(UINT_T device, UINT_T channel, CHAR_T *skill_result)
{
    if (skill_result == NULL) {
        return OPRT_INVALID_PARM;
    }
    DEVICE_MEDIA_INFO_T media_info;
    memset(&media_info, 0, sizeof(DEVICE_MEDIA_INFO_T));
    // tuya_ipc_media_adapter_get_media_info(device,channel,&media_info);
    INT_T len = sprintf(skill_result, "\\\"cloudGW\\\":%d,", 1);
    snprintf(skill_result + len, 512,
             "\\\"videos\\\":[{\\\"streamType\\\":2,\\\"codecType\\\":%d,\\\"sampleRate\\\":%d,"
             "\\\"profileId\\\":\\\"\\\",\\\"width\\\":%d,\\\"height\\\":%d},"
             "{\\\"streamType\\\":4,\\\"codecType\\\":%d,\\\"sampleRate\\\":%d,"
             "\\\"width\\\":%d,\\\"height\\\":%d}],"
             "\\\"audios\\\":[{\\\"codecType\\\":%d,\\\"sampleRate\\\":%d,"
             "\\\"dataBit\\\":%d,\\\"channels\\\":1}]",
             media_info.av_encode_info.video_codec[E_IPC_STREAM_VIDEO_MAIN],
             media_info.av_encode_info.video_freq[E_IPC_STREAM_VIDEO_MAIN],
             media_info.av_encode_info.video_width[E_IPC_STREAM_VIDEO_MAIN],
             media_info.av_encode_info.video_height[E_IPC_STREAM_VIDEO_MAIN],
             media_info.av_encode_info.video_codec[E_IPC_STREAM_VIDEO_SUB],
             media_info.av_encode_info.video_freq[E_IPC_STREAM_VIDEO_SUB],
             media_info.av_encode_info.video_width[E_IPC_STREAM_VIDEO_SUB],
             media_info.av_encode_info.video_height[E_IPC_STREAM_VIDEO_SUB],
             media_info.av_encode_info.audio_codec[E_IPC_STREAM_AUDIO_MAIN],
             media_info.av_encode_info.audio_sample[E_IPC_STREAM_AUDIO_MAIN],
             media_info.av_encode_info.audio_databits[E_IPC_STREAM_AUDIO_MAIN]);

    PR_DEBUG("CLOUD GW skill info:%s", skill_result);
    return OPRT_OK;
}

BOOL_T tuya_ipc_get_if_skill_uploaded()
{
    return sg_uploaded;
}

OPERATE_RET tuya_ipc_set_local_ai_skills(IN UINT_T local_ai_skills)
{
    g_skill_info.local_ai_skills = local_ai_skills;
    return OPRT_OK;
}

INT_T tuya_imm_get_security_ability(VOID)
{
    return ((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
}

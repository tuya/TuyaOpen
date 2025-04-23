#include <stdio.h>
#include "websocket_client.h"
#include "tuya_speaker_voice_gw.h"
#include "tuya_iot_config.h"
#include "tal_api.h"
// #include "tal_hash.h"
#include "mix_method.h"
#include "websocket_client.h"
#include "protobuf_utils.h"
#include "aispeech.pb-c.h"
#include "tuya_voice_protocol.h"
#include "tuya_iot.h"

#define WS_PSK_PORT                 1443
#define TY_KEY_VOICE_GW_DOMAIN      "voice_gw_domain_name"
#define TY_ATOP_GET_VOICE_GW_DOMAIN "tuya.device.aispeech.gateway.ws.domain"

struct domain_name_timer {
    TIMER_ID tm_msg;
    TIME_MS tm_val;
};

typedef struct {
    TUYA_SPEAKER_WS_CB bin_cb;
    TUYA_SPEAKER_WS_CB text_cb;
} TUYA_SPEAKER_WS_CBS_S;

static struct domain_name_timer *p_domain_name_tm = NULL;
static char s_domain_name_value[HTTP_URL_LMT + 1] = {0};
static WEBSOCKET_HANDLE_T s_ws_hdl = NULL;
static TUYA_SPEAKER_WS_CBS_S s_ws_cbs = {0};
static uint32_t s_ws_keepalive = 0;

static OPERATE_RET __generate_tid(char *tid, int len)
{
    tuya_iot_client_t *iot_client = tuya_iot_client_get();
    uint8_t i = 0, random[6] = {0};
    TUYA_CHECK_NULL_RETURN(tid, OPRT_INVALID_PARM);
    if (iot_client == NULL || strlen(iot_client->config.uuid) == 0) {
        PR_ERR("gw uuid is invalid");
        return OPRT_COM_ERROR;
    }

    /*example for tid, which is a uuid.
        09c40364-fed3-4a31-abcb-fb3f8d009136  (32+4)bytes
        tuya4ca05e485e1e40f0-xx-xx-xxxx-xxxx  (20+12+4)bytes
    */
    for (i = 0; i < sizeof(random); i++) {
        random[i] = (uint8_t)tal_system_get_random(0xFF);
    }
    int written = snprintf(tid, len, "%s-%02x-%02x-%02x%02x-%02x%02x", iot_client->config.uuid, random[0], random[1],
                           random[2], random[3], random[4], random[5]);
    if (written >= len) {
        PR_ERR("tid buffer is too small");
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    return OPRT_OK;
}

static OPERATE_RET __generate_signature(uint8_t *signature)
{
    uint8_t hmac[32] = {0};
    size_t local_key_len = 0, virtual_id_len = 0;
    tuya_iot_client_t *iot_client = tuya_iot_client_get();

    TUYA_CHECK_NULL_RETURN(signature, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(iot_client, OPRT_INVALID_PARM);

    local_key_len = strlen(iot_client->activate.localkey);
    virtual_id_len = strlen(iot_client->activate.devid);
    if (local_key_len == 0 || virtual_id_len == 0) {
        PR_ERR("gw local key or virtual id is invalid");
        return OPRT_COM_ERROR;
    }

    tal_sha256_mac((uint8_t *)iot_client->activate.localkey, local_key_len, (uint8_t *)iot_client->activate.devid,
                   virtual_id_len, hmac);
    byte2str(signature, hmac, sizeof(hmac), 0);

    return OPRT_OK;
}

static char *__generate_uri(char *uri, int len)
{
    OPERATE_RET rt = OPRT_OK;
    char tid[36 + 1] = {0};
    uint8_t signature[128] = {0};
    TUYA_CHECK_NULL_RETURN(uri, NULL);

    tuya_iot_client_t *iot_client = tuya_iot_client_get();
    TUYA_CALL_ERR_RETURN_VAL(__generate_tid(tid, sizeof(tid)), NULL);
    TUYA_CALL_ERR_RETURN_VAL(__generate_signature(signature), NULL);
    if (s_ws_keepalive) {
        snprintf(uri, len, "wss://%s/AISpeech?role=device&username=%s&authorization=%s&version=0.1&tid=%s&keepalive=%d",
                 s_domain_name_value, iot_client->activate.devid, signature, tid, s_ws_keepalive);
    } else {
        snprintf(uri, len, "wss://%s/AISpeech?role=device&username=%s&authorization=%s&version=0.1&tid=%s",
                 s_domain_name_value, iot_client->activate.devid, signature, tid);
    }

    PR_DEBUG("stream gateway uri is: %s", uri);

    return uri;
}

static OPERATE_RET __speaker_ws_client_start(void)
{
    char uri[512] = {0};
    WEBSOCKET_CLIENT_CFG_S ws_cfg = {0};
    OPERATE_RET rt = OPRT_OK;

    memset(&ws_cfg, 0, sizeof(ws_cfg));
    ws_cfg.uri = __generate_uri(uri, sizeof(uri));
    ws_cfg.handshake_conn_timeout = VOICE_PROTOCOL_STREAM_GW_HANDSHAKE_CONN_TIMEOUT;
    ws_cfg.handshake_recv_timeout = VOICE_PROTOCOL_STREAM_GW_HANDSHAKE_RECV_TIMEOUT;
    ws_cfg.reconnect_wait_time = VOICE_PROTOCOL_STREAM_GW_RECONNECT_WAIT_TIME;
    ws_cfg.recv_bin_cb = s_ws_cbs.bin_cb;
    ws_cfg.recv_text_cb = s_ws_cbs.text_cb;
    ws_cfg.keep_alive_time = s_ws_keepalive * 1000;

    PR_DEBUG("websocket client %p previous", s_ws_hdl);
    // if (NULL != s_ws_hdl) {
    //     websocket_client_destory(s_ws_hdl);
    // }
    PR_DEBUG("websocket_client_init ws_cfg.uri=%s", ws_cfg.uri);

    TUYA_CALL_ERR_RETURN(websocket_client_create(&s_ws_hdl, &ws_cfg));
    PR_DEBUG("websocket_client_start");
    TUYA_CALL_ERR_RETURN(websocket_client_start(s_ws_hdl));
    PR_DEBUG("websocket client %p create", s_ws_hdl);

    return OPRT_OK;
}

static OPERATE_RET __read_domain_name(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *val = NULL;
    size_t len = 0;

    rt = tal_kv_get(TY_KEY_VOICE_GW_DOMAIN, &val, &len);
    if (rt != OPRT_OK) {
        PR_WARN("read domain name failed, rt: %d", rt);
        return OPRT_NOT_FOUND;
    }
    if (len > (sizeof(s_domain_name_value) - 1) || len <= 0) {
        PR_WARN("read domain_name_len[%d] is invalid", len);
        TUYA_CALL_ERR_RETURN(tal_kv_free(val));
        return OPRT_NOT_SUPPORTED;
    } else {
        memcpy(s_domain_name_value, val, len);
        TUYA_CALL_ERR_RETURN(tal_kv_free(val));
    }
    PR_DEBUG("voice gw domain name read: %s", s_domain_name_value);

    return OPRT_OK;
}

static OPERATE_RET __write_domain_name(void)
{
    return tal_kv_set(TY_KEY_VOICE_GW_DOMAIN, (uint8_t *)s_domain_name_value, strlen(s_domain_name_value));
}

static OPERATE_RET __delete_domain_name(void)
{
    OPERATE_RET rt = OPRT_OK;
    memset(s_domain_name_value, 0, sizeof(s_domain_name_value));

    TUYA_CALL_ERR_RETURN(tal_kv_del(TY_KEY_VOICE_GW_DOMAIN));

    return OPRT_OK;
}

static OPERATE_RET __get_voice_gw_domain_name(void)
{
    OPERATE_RET rt = OPRT_OK;
    cJSON *result = NULL;

    if (__read_domain_name() == OPRT_OK) {
        return OPRT_OK;
    }
    if (TUYA_SECURITY_LEVEL == 0) {
        rt = atop_service_comm_post_simple(TY_ATOP_GET_VOICE_GW_DOMAIN, "1.0", "{\"isPsk\": true}", NULL, &result);
    } else {
        rt = atop_service_comm_post_simple(TY_ATOP_GET_VOICE_GW_DOMAIN, "1.0", NULL, NULL, &result);
    }

    if (rt != OPRT_OK) {
        PR_ERR("get voice gw domain name failed");
        return OPRT_COM_ERROR;
    }
    char *p_domain_name = cJSON_Print(result);
    if (NULL == p_domain_name) {
        PR_ERR("voice gw domain name is invalid");
        return OPRT_COM_ERROR;
    }
    /* "stream-cn.wgine.com" */
    memset(s_domain_name_value, 0, sizeof(s_domain_name_value));
    size_t domain_name_len = strlen(p_domain_name) - 2;
    if (domain_name_len > (sizeof(s_domain_name_value) - 1) || domain_name_len <= 0) {
        PR_WARN("get domain_name_len[%d] is invalid", domain_name_len);
        return OPRT_COM_ERROR;
    }
    if (TUYA_SECURITY_LEVEL == 0) {
        snprintf(s_domain_name_value, sizeof(s_domain_name_value), "%s:%d", (p_domain_name + 1), WS_PSK_PORT);
    } else {
        strncpy(s_domain_name_value, p_domain_name + 1, domain_name_len);
    }

    TUYA_CALL_ERR_RETURN(__write_domain_name());
    SAFE_FREE(p_domain_name);
    if (result)
        cJSON_Delete(result);

    PR_DEBUG("voice gw domain name is: %s", s_domain_name_value);

    return OPRT_OK;
}

static void __get_domain_name_timer_cb(TIMER_ID timer_id, void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    do {
        if (!tuya_iot_is_connected()) {
            p_domain_name_tm->tm_val = 50;
            break;
        }
        if (OPRT_OK != __get_voice_gw_domain_name()) {
            p_domain_name_tm->tm_val = 2 * 1000;
            break;
        }
        // TUYA_CALL_ERR_LOG(gw_thread_pool_release_tm_msg(p_domain_name_tm->hdl, p_domain_name_tm->tm_msg));
        tal_sw_timer_delete(p_domain_name_tm->tm_msg);
        SAFE_FREE(p_domain_name_tm);

        PR_WARN("websocket client %p will start / restart *********************", s_ws_hdl);
        // tuya_speaker_ws_client_stop();
        __speaker_ws_client_start();
        return;
    } while (0);

    TUYA_CALL_ERR_LOG(tal_sw_timer_start(p_domain_name_tm->tm_msg, p_domain_name_tm->tm_val, TAL_TIMER_ONCE));
}

static OPERATE_RET __get_domain_name_timer_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("start get domain name of voice cloud platform");
    SAFE_MALLOC(p_domain_name_tm, sizeof(struct domain_name_timer));
    p_domain_name_tm->tm_val = 10;
    p_domain_name_tm->tm_msg = NULL;
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__get_domain_name_timer_cb, NULL, &p_domain_name_tm->tm_msg));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(p_domain_name_tm->tm_msg, p_domain_name_tm->tm_val, TAL_TIMER_ONCE));
    return OPRT_OK;
}

OPERATE_RET tuya_speaker_ws_client_init(TUYA_SPEAKER_WS_CB bin_cb, TUYA_SPEAKER_WS_CB text_cb)
{
    s_ws_cbs.bin_cb = bin_cb;
    s_ws_cbs.text_cb = text_cb;
    return OPRT_OK;
}

OPERATE_RET tuya_speaker_del_domain_name(void)
{
    PR_DEBUG("__delete_domain_name");
    return __delete_domain_name();
}

OPERATE_RET tuya_speaker_ws_client_start(void)
{
    tuya_iot_client_t *iot_client = tuya_iot_client_get();
    // PR_DEBUG("gw active status: %d", gw_cntl->gw_wsm.stat);
    // PR_DEBUG("ai speech url: %s", gw_cntl->gw_actv.ai_speech_url);

    if (tuya_iot_activated(iot_client)) {
        return __get_domain_name_timer_start();
    }

    return OPRT_COM_ERROR;
}

OPERATE_RET tuya_speaker_ws_client_stop(void)
{
    PR_DEBUG("websocket client %p destory", s_ws_hdl);

    TUYA_CHECK_NULL_RETURN(s_ws_hdl, OPRT_INVALID_PARM);
    websocket_client_destory(s_ws_hdl);
    s_ws_hdl = NULL;

    return OPRT_OK;
}

OPERATE_RET tuya_speaker_ws_send_bin(uint8_t *data, uint32_t len)
{
    if (!tuya_speaker_ws_is_online())
        return OPRT_COM_ERROR;
    return websocket_client_send_bin(s_ws_hdl, (uint8_t *)data, len);
}

OPERATE_RET tuya_speaker_ws_send_text(uint8_t *data, uint32_t len)
{
    if (!tuya_speaker_ws_is_online())
        return OPRT_COM_ERROR;
    return websocket_client_send_text(s_ws_hdl, (uint8_t *)data, len);
}

static char *__dump_ws_connect_status(WS_CONN_STATE_T status)
{
    char *name[] = {
        "WS_CONN_STATE_NONE",
        "WS_CONN_STATE_FAILED",
        "WS_CONN_STATE_SUCCESS"
    };

    return status >= WS_CONN_STATE_NONE && status <= WS_CONN_STATE_SUCCESS ? name[status] : NULL;
}

BOOL_T tuya_speaker_ws_is_online(void)
{
    WS_CONN_STATE_T status = WS_CONN_STATE_NONE;

    if (websocket_client_get_conn_status(s_ws_hdl, &status) != OPRT_OK)
        return FALSE;

    if (status != WS_CONN_STATE_SUCCESS)
        PR_DEBUG("websocket client %p connect status: %d %s", s_ws_hdl, status, __dump_ws_connect_status(status));

    return ((WS_CONN_STATE_SUCCESS == status) ? TRUE : FALSE);
}

void tuya_speaker_ws_disconnect(void)
{
    websocket_client_disconnect(s_ws_hdl);
    PR_DEBUG("websocket_client_disconnect");
}

void tuya_speaker_ws_set_keepalive(uint32_t sec)
{
    if (sec > 600) {
        PR_WARN("keepalive time %d is over than max 300seconds,force to set 600 seconds", sec);
        s_ws_keepalive = 600;
    } else {
        s_ws_keepalive = sec;
    }
}
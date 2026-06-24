/**
 * @file speaker_dp.c
 * @brief DP dispatch for smart_speaker (re-platformed onto ai_components).
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * Standard DPs (volume / mic / play / quit / ctrl_group) used to be handled by
 * voice_app_dp_process(). After migrating to the ai_mode framework they are
 * handled directly here against ai_chat_main / ai_audio_player. Product DPs
 * (BT 206/5/6, ring 207) are unchanged.
 */

#include "tal_api.h"

#include "speaker_config.h"
#include "speaker_dp.h"

#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "cJSON.h"

#include "ai_chat_main.h"
#include "ai_audio_player.h"
#include "media_src.h"
#include "speaker_bt_player.h"
#include "ai_mode_speaker.h"
#include "tal_kv.h"
#include "tal_time_service.h"

/* ---- product-side state that used to live in voice_app storage ---- */
static BOOL_T sg_mic_enabled = TRUE;

typedef struct {
    BOOL_T   dnd_enable;
    uint8_t  dnd_start_hour;
    uint8_t  dnd_end_hour;
    BOOL_T   wakeup_reply; /* play a chime on wakeword */
    char     tmode[8];     /* "single" / "free" / "multi" */
    BOOL_T   dnd_manual;   /* DP4 "do not disturb" toggle (voice/app), no time window */
} speaker_ctrl_t;

static speaker_ctrl_t sg_ctrl = {
    .dnd_enable     = FALSE,
    .dnd_start_hour = 0,
    .dnd_end_hour   = 0,
    .wakeup_reply   = TRUE,
    .tmode          = SPEAKER_TALK_MODE_MULTI, /* default: single wakeword -> multi-turn */
    .dnd_manual     = FALSE,
};

#define SPK_CTRL_KV_KEY "spk_ctrl"

/* Map the stored talk-mode string onto ai_mode_speaker's behavior. */
static void __apply_talk_mode(void)
{
    AI_SPEAKER_TALK_MODE_E m =
        (strcmp(sg_ctrl.tmode, SPEAKER_TALK_MODE_SINGLE) == 0) ? AI_SPEAKER_TALK_SINGLE : AI_SPEAKER_TALK_MULTI;
    ai_mode_speaker_set_talk_mode(m);
}

static void __ctrl_save(void)
{
    tal_kv_set(SPK_CTRL_KV_KEY, (const uint8_t *)&sg_ctrl, sizeof(sg_ctrl));
}

void speaker_dp_init(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;

    if (tal_kv_get(SPK_CTRL_KV_KEY, &buf, &len) == OPRT_OK && buf && len == sizeof(sg_ctrl)) {
        memcpy(&sg_ctrl, buf, sizeof(sg_ctrl));
        sg_ctrl.tmode[sizeof(sg_ctrl.tmode) - 1] = '\0';
    }
    if (buf) {
        tal_kv_free(buf);
    }

    __apply_talk_mode();
    PR_NOTICE("speaker_dp: ctrl loaded reply=%d tmode=%s dnd=%d[%02u-%02u]", sg_ctrl.wakeup_reply, sg_ctrl.tmode,
              sg_ctrl.dnd_enable, sg_ctrl.dnd_start_hour, sg_ctrl.dnd_end_hour);
}

BOOL_T speaker_dp_in_dnd_now(void)
{
    /* Manual DP4 toggle (voice "请勿打扰" / app) suppresses regardless of clock. */
    if (sg_ctrl.dnd_manual) {
        return TRUE;
    }
    if (!sg_ctrl.dnd_enable) {
        return FALSE;
    }
    POSIX_TM_S tm = {0};
    if (tal_time_get_local_time_custom(0, &tm) != OPRT_OK) {
        return FALSE; /* time not synced yet -> don't suppress */
    }
    int h = tm.tm_hour;
    int s = sg_ctrl.dnd_start_hour;
    int e = sg_ctrl.dnd_end_hour;
    if (s == e) {
        return FALSE; /* zero-length window */
    }
    if (s < e) {
        return (h >= s && h < e) ? TRUE : FALSE; /* same-day window */
    }
    return (h >= s || h < e) ? TRUE : FALSE; /* overnight window */
}

BOOL_T speaker_dp_mic_is_enabled(void)
{
    return sg_mic_enabled;
}

void speaker_dp_set_mic_enabled(BOOL_T on)
{
    sg_mic_enabled = on;
    /* Mic off: stop feeding the uplink VAD; mic on: the active mode re-arms it. */
    ai_audio_input_wakeup_set(on ? true : false);
}

/* P1-4: deferred ring play (ring_timer, 2 ms) */
#define RING_TIMER_DELAY_MS 2u
static TIMER_ID sg_ring_timer = NULL;

static void _ring_timer_cb(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
    PR_NOTICE("DP 207 ring: playing dingdong");
    ai_audio_play_data(AI_AUDIO_CODEC_MP3, (uint8_t *)media_src_dingdong, sizeof(media_src_dingdong));
}

static int _dp_ring(void)
{
    OPERATE_RET rt;
    if (speaker_dp_in_dnd_now()) {
        PR_NOTICE("DP 207 ring: suppressed by DND");
        return OPRT_OK;
    }
    if (sg_ring_timer == NULL) {
        rt = tal_sw_timer_create(_ring_timer_cb, NULL, &sg_ring_timer);
        if (rt != OPRT_OK) {
            ai_audio_play_data(AI_AUDIO_CODEC_MP3, (uint8_t *)media_src_dingdong, sizeof(media_src_dingdong));
            return rt;
        }
    }
    if (tal_sw_timer_is_running(sg_ring_timer)) {
        return OPRT_OK;
    }
    return tal_sw_timer_start(sg_ring_timer, RING_TIMER_DELAY_MS, TAL_TIMER_ONCE);
}

/* Report DP4 (do-not-disturb) bool back so the app/cloud reflect current state. */
static int _dp_report_dnd(tuya_iot_client_t *client, BOOL_T on)
{
    if (client == NULL) {
        return OPRT_INVALID_PARM;
    }
    dp_obj_t dp_obj      = {0};
    dp_obj.id            = SPEAKER_DP_QUIT; /* DP4 = do-not-disturb for this product */
    dp_obj.type          = PROP_BOOL;
    dp_obj.value.dp_bool = on;
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

void speaker_dp_set_dnd(BOOL_T on)
{
    if (sg_ctrl.dnd_manual == on) {
        return; /* no change -> skip save + report */
    }
    PR_NOTICE("DND manual -> %d", on);
    sg_ctrl.dnd_manual = on;
    __ctrl_save();
    _dp_report_dnd(tuya_iot_client_get(), on);
}

/* ---- DP 208 ctrl_group inbound parse ---- */
static void _dp_ctrl_group_set(const char *json)
{
    if (json == NULL) {
        return;
    }
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        PR_WARN("ctrl_group json parse err");
        return;
    }

    cJSON *reply = cJSON_GetObjectItem(root, SPEAKER_CTRL_REPLY);
    if (cJSON_IsBool(reply)) {
        sg_ctrl.wakeup_reply = cJSON_IsTrue(reply) ? TRUE : FALSE;
    }

    cJSON *tmode = cJSON_GetObjectItem(root, SPEAKER_CTRL_TMODE);
    if (cJSON_IsString(tmode) && tmode->valuestring) {
        strncpy(sg_ctrl.tmode, tmode->valuestring, sizeof(sg_ctrl.tmode) - 1);
        sg_ctrl.tmode[sizeof(sg_ctrl.tmode) - 1] = '\0';
    }

    cJSON *dnd = cJSON_GetObjectItem(root, SPEAKER_CTRL_DND);
    if (cJSON_IsObject(dnd)) {
        cJSON *dis = cJSON_GetObjectItem(dnd, SPEAKER_CTRL_DISTURB);
        if (cJSON_IsBool(dis)) {
            sg_ctrl.dnd_enable = cJSON_IsTrue(dis) ? TRUE : FALSE;
        }
        cJSON *st = cJSON_GetObjectItem(dnd, SPEAKER_CTRL_START);
        if (cJSON_IsString(st) && st->valuestring) {
            sg_ctrl.dnd_start_hour = (uint8_t)atoi(st->valuestring);
        }
        cJSON *en = cJSON_GetObjectItem(dnd, SPEAKER_CTRL_END);
        if (cJSON_IsString(en) && en->valuestring) {
            sg_ctrl.dnd_end_hour = (uint8_t)atoi(en->valuestring);
        }
    }

    cJSON_Delete(root);
    __apply_talk_mode();
    __ctrl_save();
    PR_NOTICE("ctrl_group set: reply=%d tmode=%s dnd=%d", sg_ctrl.wakeup_reply, sg_ctrl.tmode, sg_ctrl.dnd_enable);
}

/* ---- Main DP dispatch (called from the iot dp handler) ---- */
int speaker_dp_process(dp_obj_recv_t *dpobj)
{
    PR_DEBUG("%s", __func__);
    if (dpobj == NULL) {
        return OPRT_INVALID_PARM;
    }

    tuya_iot_client_t *client = tuya_iot_client_get();

    for (uint32_t i = 0; i < dpobj->dpscnt; i++) {
        dp_obj_t *dp = dpobj->dps + i;

        if (dp->id == SPEAKER_DP_VOLUME && dp->type == PROP_VALUE) {
            ai_chat_set_volume((int)dp->value.dp_value);
            speaker_dp_report_volume(client, (uint8_t)dp->value.dp_value);
        } else if (dp->id == SPEAKER_DP_MIC && dp->type == PROP_BOOL) {
            speaker_dp_set_mic_enabled(dp->value.dp_bool);
            speaker_dp_report_mic(client, dp->value.dp_bool);
        } else if (dp->id == SPEAKER_DP_PLAY_SWITCH && dp->type == PROP_BOOL) {
            if (!dp->value.dp_bool) {
                ai_audio_player_stop(AI_AUDIO_PLAYER_ALL);
            }
            speaker_dp_report_play_state(client);
        } else if (dp->id == SPEAKER_DP_QUIT && dp->type == PROP_BOOL) {
            /* DP4 is the "do not disturb" switch for this product (the cloud maps
             * the "请勿打扰" voice intent / app toggle onto it). */
            speaker_dp_set_dnd(dp->value.dp_bool);
        } else if (dp->id == SPEAKER_DP_CTRL_GROUP && dp->type == PROP_STR) {
            _dp_ctrl_group_set(dp->value.dp_str);
            speaker_dp_report_ctrl_group(client);
        } else if (dp->id == SPEAKER_DP_RING) {
            _dp_ring();
        } else if (dp->id == SPEAKER_DP_BT && dp->type == PROP_BOOL) {
            speaker_bt_dp_handle_enable(dp->value.dp_bool);
        } else if (dp->id == SPEAKER_DP_BT_SWITCH && dp->type == PROP_BOOL) {
            speaker_bt_dp_handle_switch(dp->value.dp_bool);
        } else if (dp->id == SPEAKER_DP_BT_VISIBLE && dp->type == PROP_BOOL) {
            speaker_bt_dp_handle_visible(dp->value.dp_bool);
        }
    }

    return OPRT_OK;
}

/* ---- DP report helpers ---- */
int speaker_dp_report_volume(tuya_iot_client_t *client, uint8_t vol)
{
    dp_obj_t dp_obj       = {0};
    dp_obj.id             = SPEAKER_DP_VOLUME;
    dp_obj.type           = PROP_VALUE;
    dp_obj.value.dp_value = vol;
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

int speaker_dp_report_mic(tuya_iot_client_t *client, BOOL_T on)
{
    dp_obj_t dp_obj      = {0};
    dp_obj.id            = SPEAKER_DP_MIC;
    dp_obj.type          = PROP_BOOL;
    dp_obj.value.dp_bool = on;
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

int speaker_dp_report_play_state(tuya_iot_client_t *client)
{
    dp_obj_t dp_obj      = {0};
    dp_obj.id            = SPEAKER_DP_PLAY_SWITCH;
    dp_obj.type          = PROP_BOOL;
    dp_obj.value.dp_bool = ai_audio_player_is_playing() ? TRUE : FALSE;
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

int speaker_dp_report_ctrl_group(tuya_iot_client_t *client)
{
    char json[256];
    int  n = snprintf(json, sizeof(json),
                      "{\"" SPEAKER_CTRL_DND "\":{"
                      "\"" SPEAKER_CTRL_DISTURB "\":%s,"
                      "\"" SPEAKER_CTRL_START "\":\"%02u\","
                      "\"" SPEAKER_CTRL_END "\":\"%02u\"},"
                      "\"" SPEAKER_CTRL_REPLY "\":%s,"
                      "\"" SPEAKER_CTRL_CTALK "\":%s,"
                      "\"" SPEAKER_CTRL_TMODE "\":\"%s\"}",
                      sg_ctrl.dnd_enable ? "true" : "false", sg_ctrl.dnd_start_hour, sg_ctrl.dnd_end_hour,
                      sg_ctrl.wakeup_reply ? "true" : "false",
                      (strcmp(sg_ctrl.tmode, SPEAKER_TALK_MODE_SINGLE) != 0) ? "true" : "false", sg_ctrl.tmode);
    if (n < 0 || n >= (int)sizeof(json)) {
        return OPRT_INVALID_PARM;
    }

    dp_obj_t dp_obj     = {0};
    dp_obj.id           = SPEAKER_DP_CTRL_GROUP;
    dp_obj.type         = PROP_STR;
    dp_obj.value.dp_str = json;
    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

int speaker_dp_report_volume_current(tuya_iot_client_t *client)
{
    return speaker_dp_report_volume(client, (uint8_t)ai_chat_get_volume());
}

int speaker_dp_report_mic_current(tuya_iot_client_t *client)
{
    return speaker_dp_report_mic(client, sg_mic_enabled);
}

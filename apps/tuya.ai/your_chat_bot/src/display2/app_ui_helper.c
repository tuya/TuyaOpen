/**
 * @file app_ui_helper.c
 * @brief app_ui_helper module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_ui_helper.h"

#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
#include "lang_config.h"

#include "ai_chat_main.h"

#include "tal_time_service.h"
#include "tuya_iot.h"
#include "tuya_lvgl.h"

#include "screens/ui_setting.h"
#include "ui.h"

#include "tal_api.h"
#include "netmgr.h"
#include <stdint.h>

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TIMER_ID s_volume_timer_id = NULL;
static TIMER_ID s_date_timer_id   = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

uint8_t app_ui_get_volume_value(void)
{
    int volume = 0;
    ai_audio_player_get_vol(&volume);
    return volume;
}

static void __app_ui_set_volume_value_tm_cb(TIMER_ID timer_id, void *arg)
{
    uint8_t value = *(uint8_t *)arg;
    ai_audio_player_set_vol(value);

    // Update DP
    extern OPERATE_RET ai_audio_volume_upload(void);
    ai_audio_volume_upload();

    char volume_msg[36] = {0};
    snprintf(volume_msg, sizeof(volume_msg), "%s %d (UI)", SYSTEM_MSG_VOLUME, value);
    tuya_lvgl_mutex_lock();
    ui_set_system_msg(volume_msg);
    tuya_lvgl_mutex_unlock();
}

void app_ui_set_volume_value(uint8_t value)
{
    static uint8_t s_volume_value = 0;

    s_volume_value = value;

    if (NULL == s_volume_timer_id) {
        tal_sw_timer_create(__app_ui_set_volume_value_tm_cb, &s_volume_value, &s_volume_timer_id);
    }

    if (s_volume_timer_id) {
        tal_sw_timer_start(s_volume_timer_id, 300, TAL_TIMER_ONCE);
    }
}

static void __app_ui_get_date_tm_cb(TIMER_ID timer_id, void *arg)
{
    uint32_t year = 0, month = 0, day = 0;
    uint32_t hour = 0, minute = 0;

    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);
    hour   = tm.tm_hour;
    minute = tm.tm_min;
    year   = tm.tm_year + 1900;
    month  = tm.tm_mon + 1;
    day    = tm.tm_mday;

    PR_DEBUG("date: %04d-%02d-%02d, time: %02d:%02d", year, month, day, hour, minute);

    // update date time display
    tuya_lvgl_mutex_lock();
    ui_setting_date_update(year, month, day);
    ui_setting_time_update(hour, minute);
    tuya_lvgl_mutex_unlock();

    app_ui_get_date_time_loop_start();
}

static uint8_t __app_ui_date_get_next_time(void)
{
    POSIX_TM_S tm = {0};

    tal_time_get_local_time_custom(0, &tm);

    return 60 - tm.tm_sec;
}

void app_ui_get_date_time_loop_start(void)
{
    if (NULL == s_date_timer_id) {
        tal_sw_timer_create(__app_ui_get_date_tm_cb, NULL, &s_date_timer_id);
    }

    if (s_date_timer_id) {
        uint8_t next_time = __app_ui_date_get_next_time();
        tal_sw_timer_start(s_date_timer_id, next_time * 1000 + 300, TAL_TIMER_ONCE);
    }
}

void app_ui_get_date_time_loop_stop(void)
{
    if (s_date_timer_id) {
        tal_sw_timer_stop(s_date_timer_id);
    }
}

static int __app_ui_time_sync_cb(void *data)
{
    __app_ui_get_date_tm_cb(NULL, NULL);
    return 0;
}

void app_ui_get_date(uint32_t *year, uint32_t *month, uint32_t *day)
{

    OPERATE_RET rt = tal_time_check_time_sync();
    if (rt != OPRT_OK) {
        // subscribe time sync event
        tal_event_subscribe("app.time.sync", "app_ui_helper", __app_ui_time_sync_cb, SUBSCRIBE_TYPE_ONETIME);
        return;
    }

    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);
    *year  = tm.tm_year + 1900;
    *month = tm.tm_mon + 1;
    *day   = tm.tm_mday;

    PR_DEBUG("date: %04d-%02d-%02d", *year, *month, *day);

    return;
}

void app_ui_get_time(uint32_t *hour, uint32_t *minute)
{
    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);
    *hour   = tm.tm_hour;
    *minute = tm.tm_min;

    PR_DEBUG("time: %02d:%02d", *hour, *minute);

    return;
}

void app_ui_get_wifi_status(uint8_t *status)
{
    netmgr_status_e net_status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &net_status);
    if (net_status == NETMGR_LINK_UP) {
        *status = 1;
    } else {
        *status = 0;
    }

    return;
}

static int __app_ui_network_status_change_cb(void *data)
{
    netmgr_status_e net_status = *(netmgr_status_e *)data;
    uint8_t         connected  = (net_status == NETMGR_LINK_UP) ? 1 : 0;

    tuya_lvgl_mutex_lock();
    ui_setting_wifi_update(connected);
    ui_set_system_msg(connected ? SYSTEM_MSG_WIFI_SSID : SYSTEM_MSG_WIFI_DISCONNECTED);
    tuya_lvgl_mutex_unlock();

    return 0;
}

void app_ui_network_status_change_subscribe(void)
{
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "app_ui_helper", __app_ui_network_status_change_cb,
                        SUBSCRIBE_TYPE_NORMAL);
}

void app_ui_network_status_change_unsubscribe(void)
{
    tal_event_unsubscribe(EVENT_LINK_STATUS_CHG, "app_ui_helper", __app_ui_network_status_change_cb);
}

void app_ui_reset_device(void)
{
    tuya_iot_reset(tuya_iot_client_get());
}

/* waveform power function start */
#include <math.h>

// Audio power estimation
#define AUDIO_POWER_BUFFER_SIZE   160
#define AUDIO_POWER_NORMALIZATION 50000.0f

static int16_t      sg_audio_power_buffer[AUDIO_POWER_BUFFER_SIZE] = {0};
static float        sg_audio_power                                 = 0.0f;
static MUTEX_HANDLE sg_audio_power_mutex                           = NULL;

/**
 * @brief Calculate audio power from PCM samples
 */
void app_ui_helper_calculate_audio_power(uint8_t *audio_data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;
    if (NULL == sg_audio_power_mutex) {
        rt = tal_mutex_create_init(&sg_audio_power_mutex);
        if (OPRT_OK != rt) {
            PR_ERR("Failed to create audio power mutex");
            return;
        }
    }

    if (audio_data == NULL || data_len == 0) {
        return;
    }

    uint32_t num_samples = data_len / 2; // 16-bit samples = 2 bytes per sample
    if (num_samples > AUDIO_POWER_BUFFER_SIZE) {
        num_samples = AUDIO_POWER_BUFFER_SIZE;
    }

    int16_t *pcm_samples = (int16_t *)audio_data;

    // Update circular buffer
    if (num_samples >= AUDIO_POWER_BUFFER_SIZE) {
        memcpy(sg_audio_power_buffer, pcm_samples, AUDIO_POWER_BUFFER_SIZE * sizeof(int16_t));
    } else {
        memmove(sg_audio_power_buffer, &sg_audio_power_buffer[num_samples],
                (AUDIO_POWER_BUFFER_SIZE - num_samples) * sizeof(int16_t));
        memcpy(&sg_audio_power_buffer[AUDIO_POWER_BUFFER_SIZE - num_samples], pcm_samples,
               num_samples * sizeof(int16_t));
    }

    // Calculate max absolute value
    float max_value = 0.0f;
    for (int i = 0; i < AUDIO_POWER_BUFFER_SIZE; i++) {
        float abs_sample = fabsf((float)sg_audio_power_buffer[i]);
        if (abs_sample > max_value) {
            max_value = abs_sample;
        }
    }

    // Normalize
    float normalized_power = max_value / AUDIO_POWER_NORMALIZATION;
    if (normalized_power > 1.0f)
        normalized_power = 1.0f;
    if (normalized_power < 0.0f)
        normalized_power = 0.0f;

    // Update power with mutex protection
    if (sg_audio_power_mutex != NULL) {
        tal_mutex_lock(sg_audio_power_mutex);
        sg_audio_power = normalized_power;
        tal_mutex_unlock(sg_audio_power_mutex);
    }
}

float app_ui_helper_get_audio_power(void)
{
    static float    power_max          = 0.0f;
    static float    power_min          = 0.0f;
    static float    smoothed_power     = 0.0f;  // smoothed power value
    static float    display_value      = 0.0f;  // final display value (with inertia)
    static float    adaptive_threshold = 0.01f; // adaptive threshold
    static uint32_t frame_count        = 0;     // frame counter
    float           power              = 0.0f;

    if (sg_audio_power_mutex != NULL) {
        tal_mutex_lock(sg_audio_power_mutex);
        power = sg_audio_power;
        tal_mutex_unlock(sg_audio_power_mutex);
    }

    // PR_DEBUG("---> audio power: %f", power);

    // update power max and min
    if (power > power_max) {
        power_max = power;
    }
    if (power < power_min) {
        power_min = power;
    }

    // update adaptive threshold every 100 frames (about 3 seconds at 33fps)
    frame_count++;
    if (frame_count >= 100) {
        // use 15% of the maximum value as reference, but at least 0.003
        adaptive_threshold = power_max * 0.15f;
        if (adaptive_threshold < 0.003f) {
            adaptive_threshold = 0.003f;
        }
        // gradually decay power_max, to avoid long-term impact after one large volume
        power_max *= 0.9f;
        frame_count = 0;
    }

    // PR_DEBUG("---> audio power: %f, power_max: %f, threshold: %f", power, power_max, adaptive_threshold);

    // 1. dynamic range adjustment - normalize according to adaptive threshold
    // use smaller divisor for more sensitivity
    float normalized_power = power / (adaptive_threshold * 1.2f);
    if (normalized_power > 1.0f) {
        normalized_power = 1.0f;
    }

    // 2. nonlinear mapping - use power curve to amplify changes
    // square for values < 0.5 to boost small signals
    // direct use for values >= 0.5 to preserve dynamics
    float enhanced_power;
    if (normalized_power < 0.5f) {
        // amplify small values: x^0.7 (between sqrt and linear)
        enhanced_power = powf(normalized_power, 0.7f);
    } else {
        // preserve large values with slight boost
        enhanced_power = 0.5f * powf(0.5f, 0.7f) + (normalized_power - 0.5f) * 1.3f;
        if (enhanced_power > 1.0f) {
            enhanced_power = 1.0f;
        }
    }

    // 3. smoothing filter - exponential moving average (EMA)
    // alpha = 0.6 means new value weight 60%, history value weight 40% (more responsive)
    const float alpha = 0.6f;
    smoothed_power    = alpha * enhanced_power + (1.0f - alpha) * smoothed_power;

    // 4. inertia effect - rise very fast(0.85), fall medium(0.3)
    float target_value = smoothed_power;
    if (target_value > display_value) {
        // rise very fast for immediate response
        display_value = 0.85f * target_value + 0.15f * display_value;
    } else {
        // fall at medium speed for natural decay
        display_value = 0.3f * target_value + 0.7f * display_value;
    }

    // 5. add minimum animation threshold, below which display is 0
    if (display_value < 0.03f) {
        display_value *= 0.7f; // faster decay to 0
    }

    // final limit range
    if (display_value > 1.0f) {
        display_value = 1.0f;
    }
    if (display_value < 0.0f) {
        display_value = 0.0f;
    }

    // PR_DEBUG("---> display_value: %f", display_value);

    return display_value;
}

/* waveform power function end */
#endif
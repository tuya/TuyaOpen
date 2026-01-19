/**
 * @file game_pet.c
 * @brief
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

/*============================ INCLUDES ======================================*/
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_kv.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "tal_sw_timer.h"
#include "ai_audio_player.h"
#include "game_pet.h"
#include "media_pet.h"
#include "menu_info_screen.h"
#include "main_screen.h"
#include "lv_vendor.h"

#define PET_DEBUG_ENABLE 0

extern void pocket_game_pet_indev_init(void);
extern OPERATE_RET game_ai_chat_init(void);

/*============================ MACROS ========================================*/
#define DEFAULT_STATE_VALUE 70
#define KVKEY_GAME_PET_STATE "GAME_PET_STATE"
#define DPID_HAPPINESS 102
#define DPID_CLEANNESS 103
#define DPID_HEALTH 104
#define DPID_ENERGY 105
#define DPID_MOOD 107
#define PET_EVENT_TIMER PET_EVENT_MAX
#define PET_EVENT_TIMER PET_EVENT_MAX
#define PET_OPT_TOTAL (PET_EVENT_TIMER+1)
#define PET_TIMER_ONCE_MS (3000)  // 1000 * 3

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
#define PET_TIMER_CYCLE_MS (60000)  // 1000 * 60
#else
#define PET_TIMER_CYCLE_MS (1200000)  // 1000 * 60 * 20
#endif

/*============================ LOCAL VARIABLES ===============================*/
static const int s_pet_opt_values[PET_OPT_TOTAL][PET_STATE_TOTAL] = {
    //                        health, energy, cleaness, happiness
    [PET_EVENT_FEED_HAMBURGER] = {-1,     8,    -1,     0},
    [PET_EVENT_DRINK_WATER]    = { 1,     2,    -2,     1},
    [PET_EVENT_FEED_PIZZA]     = {-1,     6,    -3,     2},
    [PET_EVENT_FEED_APPLE]     = { 1,     1,     0,     1},
    [PET_EVENT_FEED_FISH]      = { 1,     3,    -1,     0},
    [PET_EVENT_FEED_CARROT]    = { 2,     1,     0,    -2},
    [PET_EVENT_FEED_ICE_CREAM] = { 0,     3,    -2,     3},
    [PET_EVENT_FEED_COOKIE]    = { 0,     3,    -2,     0},
    [PET_EVENT_TOILET]         = { 0,    -1,    -3,     1},
    [PET_EVENT_TAKE_BATH]      = { 0,    -2,    10,     3},
    [PET_EVENT_SEE_DOCTOR]     = {10,    -1,    -2,    -5},
    [PET_EVENT_SLEEP]          = { 3,    10,     0,     1},
    [PET_EVENT_WAKE_UP]        = { 1,    10,    -2,     2},
    [PET_STAT_RANDOMIZE]       = { 0,     0,     0,     0},
    [PET_EVENT_TIMER]          = {-1,    -3,    -2,    -4}
};

static const int s_pet_dp_values[PET_STATE_TOTAL] = {
    [PET_S_HEALTH_INDEX]     = DPID_HEALTH,
    [PET_S_ENERGY_INDEX]     = DPID_ENERGY,
    [PET_S_CLEAN_INDEX]      = DPID_CLEANNESS,
    [PET_S_HAPPINESS_INDEX]  = DPID_HAPPINESS
};

static TIMER_ID s_pet_timer_cycle_id = NULL;
static TIMER_ID s_pet_timer_once_id = NULL;
static int *s_pet_state = NULL;
static pet_mood_dp_value_t s_pet_mood_dp_value = MODE_DP_HAPPY;

/*============================ IMPLEMENTATION ================================*/
// display pet state on lvgl
void _display_pet_state(ai_pet_state_t pet_state)
{
    switch (pet_state) {
        case AI_PET_STATE_SLEEP: {
            // game_pet_play_alert(AI_AUDIO_LOADING_TONE);
            game_pet_play_alert(PET_ALERT_CANCEL_FAIL_TRI_TONE);
        } break;
        case AI_PET_STATE_DANCE: {
            game_pet_play_alert(PET_ALERT_CANCEL_FAIL_TRI_TONE);
        } break;
        case AI_PET_STATE_EAT: {
            // game_pet_play_alert(AI_AUDIO_CONFIRM);
            game_pet_play_alert(PET_ALERT_SHORT_SELECT_TONE);
        } break;
        case AI_PET_STATE_BATH: {
            // game_pet_play_alert(AI_AUDIO_DOWNWARD_BI_TONE);
            game_pet_play_alert(PET_ALERT_FAIL_CANCEL_BI_TONE);
        } break;
        case AI_PET_STATE_TOILET: {
            game_pet_play_alert(PET_ALERT_FAIL_CANCEL_BI_TONE);
        } break;
        case AI_PET_STATE_SICK: {
            game_pet_play_alert(PET_ALERT_LOADING_TONE);
        } break;
        case AI_PET_STATE_HAPPY: {
            game_pet_play_alert(PET_ALERT_SHORT_SELECT_TONE);
        } break;
        case AI_PET_STATE_ANGRY: {
            game_pet_play_alert(PET_ALERT_THREE_STAGE_UP_TONE);
        } break;
        case AI_PET_STATE_CRY: {
            game_pet_play_alert(PET_ALERT_THREE_STAGE_UP_TONE);
        } break;
        default:
            break;
    }

    lv_vendor_disp_lock();
    main_screen_set_pet_animation_state(pet_state);
    lv_vendor_disp_unlock();
}

// show
OPERATE_RET game_pet_show(int *state)
{
    if (state == NULL) {
        return OPRT_INVALID_PARM;
    }

    PR_INFO("Game Pet State - Health: %d, Energy: %d, Cleaness: %d, Happiness: %d",
            state[PET_S_HEALTH_INDEX], state[PET_S_ENERGY_INDEX],
            state[PET_S_CLEAN_INDEX], state[PET_S_HAPPINESS_INDEX]);

    ai_pet_state_t pet_state = AI_PET_STATE_NORMAL;
    s_pet_mood_dp_value = MODE_DP_HAPPY;

    int happiness = state[PET_S_HAPPINESS_INDEX];
    if (happiness < 10) {
        PR_DEBUG("Pet is hopelessness.");
        pet_state = AI_PET_STATE_CRY;
        s_pet_mood_dp_value = MODE_DP_SAD;
    } else if (happiness < 50) {
        PR_DEBUG("Pet is sad.");
        pet_state = AI_PET_STATE_ANGRY;
        s_pet_mood_dp_value = MODE_DP_BORED;
    } else if (happiness > 80) {
        PR_DEBUG("Pet is very happy.");
        pet_state = AI_PET_STATE_DANCE;
        s_pet_mood_dp_value = MODE_DP_EXCITED;
    }

    int clean = state[PET_S_CLEAN_INDEX];
    if (clean < 20) {
        PR_DEBUG("Pet is dirty.");
        pet_state = AI_PET_STATE_ANGRY;
        s_pet_mood_dp_value = MODE_DP_SAD;
    } else if (clean < 60) {
        PR_DEBUG("Pet need shower.");
        pet_state = AI_PET_STATE_CRY;
        s_pet_mood_dp_value = MODE_DP_BORED;
    }

    int energy = state[PET_S_ENERGY_INDEX];
    if (energy < 30) {
        PR_DEBUG("Pet is hungry.");
        pet_state = AI_PET_STATE_SICK;
        s_pet_mood_dp_value = MODE_DP_BORED;
    } else if (energy > 80) {
        PR_DEBUG("Pet need exercise.");
        pet_state = AI_PET_STATE_ANGRY;
    }

    int health = state[PET_S_HEALTH_INDEX];
    if (health < 10) {
        PR_DEBUG("Pet deadth.");
        pet_state = AI_PET_STATE_SICK;
        s_pet_mood_dp_value = MODE_DP_ILL;
    } else if (health < 30) {
        PR_DEBUG("Pet is ill.");
        pet_state = AI_PET_STATE_CRY;
        s_pet_mood_dp_value = MODE_DP_ILL;
    }

    _display_pet_state(pet_state);

    // report mood dp
    dp_obj_t dps = {
        .id = DPID_MOOD,
        .type = PROP_ENUM,
        .value = (dp_value_t)(uint32_t)s_pet_mood_dp_value
    };
    tuya_iot_client_t *client = tuya_iot_client_get();
    const char *devid = tuya_iot_devid_get(client);
    tuya_iot_dp_obj_report(client, devid, &dps, 1, 0);
    return OPRT_OK;
}

// save data
OPERATE_RET game_pet_data_save(int *state)
{
    return tal_kv_set(
        KVKEY_GAME_PET_STATE,
        (const uint8_t *)state,
        sizeof(int) * PET_STATE_TOTAL
    );
}

// dp report
OPERATE_RET game_pet_data_report(int *state)
{
    if (state == NULL) {
        return OPRT_INVALID_PARM;
    }

    int i = 0;
    tuya_iot_client_t *client = tuya_iot_client_get();
    const char *devid = tuya_iot_devid_get(client);
    if (devid == NULL) {
        PR_ERR("Device ID is NULL");
        return OPRT_INVALID_PARM;
    }

    dp_obj_t dps[PET_STATE_TOTAL] = {0};

    for (i = 0; i < PET_STATE_TOTAL; i++) {
        dps[i].id = s_pet_dp_values[i];
        dps[i].type = PROP_VALUE;
        dps[i].value = (dp_value_t)(state[i]);
    }

    tuya_iot_dp_obj_report(client, devid, dps, PET_STATE_TOTAL, 0);

    return OPRT_OK;
}

// dp report
OPERATE_RET game_pet_update_state_to_menu(int *state)
{
    pet_stats_t menu_state = {
        .health = state[PET_S_HEALTH_INDEX],
        .hungry = state[PET_S_ENERGY_INDEX],
        .clean = state[PET_S_CLEAN_INDEX],
        .happy = state[PET_S_HAPPINESS_INDEX],
        .age_days = 1000,
        .weight_kg = 1000.0
    };

    main_screen_update_pet_stats(&menu_state);
    return OPRT_OK;
}

// set data
OPERATE_RET game_pet_data_add(game_pet_state_id_t idx, int value)
{
    if (s_pet_state == NULL || (int)idx < 0 || idx >= PET_STATE_TOTAL) {
        return OPRT_INVALID_PARM;
    }

    int new_value = s_pet_state[idx] + value;
    new_value = (new_value > 100) ? 100 : ((new_value < 0) ? 0 : new_value);
    s_pet_state[idx] = new_value;

    game_pet_data_save(s_pet_state);
    game_pet_update_state_to_menu(s_pet_state);
    game_pet_data_report(s_pet_state);
    game_pet_show(s_pet_state);

    return OPRT_OK;
}

// pet operation
OPERATE_RET game_pet_operation(pet_event_type_t idx, bool show_now)
{
    if (s_pet_state == NULL || (int)idx < 0 || idx >= PET_OPT_TOTAL) {
        return OPRT_INVALID_PARM;
    }

    const int *value_list = s_pet_opt_values[idx];
    int i = 0, new_value = 0;
    for (i = 0; i < PET_STATE_TOTAL; i++) {
        new_value = s_pet_state[i] + value_list[i];
        new_value = (new_value > 100) ? 100 : ((new_value < 0) ? 0 : new_value);
        s_pet_state[i] = new_value;
    }

    game_pet_data_save(s_pet_state);
    game_pet_update_state_to_menu(s_pet_state);
    game_pet_data_report(s_pet_state);
    if (show_now) {
        game_pet_show(s_pet_state);
    } else {
        tal_sw_timer_start(s_pet_timer_once_id, PET_TIMER_ONCE_MS, TAL_TIMER_ONCE);
    }

    return OPRT_OK;
}

// reset
OPERATE_RET game_pet_reset(void)
{
    if (s_pet_state == NULL) {
        return OPRT_INVALID_PARM;
    }

    // Reset all states to default value
    for (int i = 0; i < PET_STATE_TOTAL; i++) {
        s_pet_state[i] = DEFAULT_STATE_VALUE;
    }

    PR_DEBUG("Reset game pet state to default values: %d.", DEFAULT_STATE_VALUE);
    game_pet_data_save(s_pet_state);
    game_pet_update_state_to_menu(s_pet_state);
    game_pet_data_report(s_pet_state);
    game_pet_show(s_pet_state);

    return OPRT_OK;
}

// debug
OPERATE_RET game_pet_random_state(void)
{
    uint32_t rand_value = tal_system_get_random(50);
    if (rand_value == 0) {
        rand_value = 1;
    }

    int rand_state = (int)(rand_value % PET_STATE_TOTAL);
    int rand_operation = (int)(rand_value % 2);
    int final_value = (rand_operation == 0) ? (int)rand_value : -(int)rand_value;

    PR_DEBUG("Random state [%d] updated: %d.", rand_state, final_value);

    game_pet_data_add((game_pet_state_id_t)rand_state, final_value);

    return OPRT_OK;
}

 static void pet_event_callback(pet_event_type_t event_type, void *user_data)
 {
    if ((int)event_type < 0 || event_type >= PET_EVENT_MAX) {
        PR_ERR("Invalid pet event type: %d", event_type);
        return;
    }

    PR_DEBUG("Pet event callback triggered: %d", event_type);

    if (PET_STAT_RANDOMIZE == event_type) {
        game_pet_random_state();
        return;
    }

    ai_pet_state_t pet_state = AI_PET_STATE_NORMAL;
    switch (event_type) {
        case PET_EVENT_FEED_HAMBURGER:
        case PET_EVENT_DRINK_WATER:
        case PET_EVENT_FEED_PIZZA:
        case PET_EVENT_FEED_APPLE:
        case PET_EVENT_FEED_FISH:
        case PET_EVENT_FEED_CARROT:
        case PET_EVENT_FEED_ICE_CREAM:
        case PET_EVENT_FEED_COOKIE:
            pet_state = AI_PET_STATE_EAT;
            break;
        case PET_EVENT_TOILET:
            pet_state = AI_PET_STATE_TOILET;
            break;
        case PET_EVENT_TAKE_BATH:
            pet_state = AI_PET_STATE_BATH;
            break;
        case PET_EVENT_SEE_DOCTOR:
            pet_state = AI_PET_STATE_CRY;
            break;
        case PET_EVENT_SLEEP:
            pet_state = AI_PET_STATE_SLEEP;
            break;
        case PET_EVENT_WAKE_UP:
            pet_state = AI_PET_STATE_DANCE;
            break;
        default:
            PR_ERR("Unhandled pet event type: %d", event_type);
            return;
    }

    // Update pet area animation based on event type
    _display_pet_state(pet_state);

    game_pet_operation(event_type, false);
 }


static void __timer_cb(TIMER_ID timer_id, void *arg)
{
    if (s_pet_state == NULL) {
        PR_ERR("Pet state not initialized, ignoring timer callback");
        return;
    }

    if (s_pet_timer_once_id == timer_id) {
        PR_NOTICE("pet timer once callback");
        game_pet_show(s_pet_state);
    } else if (s_pet_timer_cycle_id == timer_id) {
        PR_NOTICE("pet timer cycle callback");
        game_pet_operation(PET_EVENT_TIMER, true);
    }
}

static void __game_display_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    screens_init();

    lv_vendor_start(5, 1024*8);
}

// init
OPERATE_RET game_pet_init(void)
{
    // Initialize the game pet state
    OPERATE_RET rt = OPRT_OK;
    size_t readlen = 0;

    __game_display_init();

    TUYA_CALL_ERR_RETURN(game_ai_chat_init());

    s_pet_state = (int *)tal_malloc(sizeof(int) * PET_STATE_TOTAL);
    if (NULL == s_pet_state) {
        return OPRT_MALLOC_FAILED;
    }

    // initialize state from KV storage or set to default values
    uint8_t *temp_buf = NULL;
    if ((OPRT_OK == tal_kv_get(KVKEY_GAME_PET_STATE, &temp_buf, &readlen))
        && (readlen == sizeof(int) * PET_STATE_TOTAL) && (temp_buf != NULL)) {
        memcpy(s_pet_state, temp_buf, readlen);
        tal_free(temp_buf);
        PR_INFO("Game pet initialized with KV state.");
    } else {
        if (temp_buf != NULL) {
            tal_free(temp_buf);
        }
        game_pet_reset();
        PR_WARN("Game pet initialized with default state.");
    }

    // initialize timer
    rt = tal_sw_timer_init();
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize timer: %d", rt);
        tal_free(s_pet_state);
        s_pet_state = NULL;
        return rt;
    }

    rt = tal_sw_timer_create(__timer_cb, NULL, &s_pet_timer_once_id);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create once timer: %d", rt);
        tal_free(s_pet_state);
        s_pet_state = NULL;
        return rt;
    }

    rt = tal_sw_timer_create(__timer_cb, NULL, &s_pet_timer_cycle_id);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create cycle timer: %d", rt);
        tal_free(s_pet_state);
        s_pet_state = NULL;
        return rt;
    }

    rt = tal_sw_timer_start(s_pet_timer_cycle_id, PET_TIMER_CYCLE_MS, TAL_TIMER_CYCLE);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to start cycle timer: %d", rt);
    }

    main_screen_register_pet_event_callback(pet_event_callback, NULL);

    pocket_game_pet_indev_init();

    return OPRT_OK;
}

OPERATE_RET game_pet_play_alert(PET_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *audio_data = NULL;
    uint32_t audio_size = 0;

    switch(type) {
        case PET_ALERT_BI_TONE: {
            audio_data = (uint8_t *)media_src_bi_tone_alert;
            audio_size = sizeof(media_src_bi_tone_alert);
        } 
        break;
        case PET_ALERT_CANCEL_FAIL_TRI_TONE: {
            audio_data = (uint8_t *)media_src_cancel_fail_tri_tone;
            audio_size = sizeof(media_src_cancel_fail_tri_tone);
        } 
        break;
        case PET_ALERT_CONFIRM: {
            audio_data = (uint8_t *)media_src_comfirm;
            audio_size = sizeof(media_src_comfirm);
        } 
        break;
        case PET_ALERT_DOWNWARD_BI_TONE: {
            audio_data = (uint8_t *)media_src_downward_bi_tone;
            audio_size = sizeof(media_src_downward_bi_tone);
        } 
        break;
        case PET_ALERT_FAIL_CANCEL_BI_TONE: {
            audio_data = (uint8_t *)media_src_fail_cancel_bi_tone;
            audio_size = sizeof(media_src_fail_cancel_bi_tone);
        } 
        break;
        case PET_ALERT_LOADING_TONE: {
            audio_data = (uint8_t *)media_src_loading_tone;
            audio_size = sizeof(media_src_loading_tone);
        } 
        break;
        case PET_ALERT_SHORT_SELECT_TONE: {
            audio_data = (uint8_t *)media_src_short_select_tone;
            audio_size = sizeof(media_src_short_select_tone);
        } 
        break;
        case PET_ALERT_THREE_STAGE_UP_TONE: {
            audio_data = (uint8_t *)media_src_three_stage_up_tone;
            audio_size = sizeof(media_src_three_stage_up_tone);
        } 
        break;
        default:
            break;
    }

    TUYA_CALL_ERR_RETURN(ai_audio_play_data(AI_AUDIO_CODEC_MP3, audio_data, audio_size));

    return rt;
}

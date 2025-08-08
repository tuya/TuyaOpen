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

#define PET_DEBUG_ENABLE 1

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
#include "tdl_button_manage.h"
#endif

/*============================ MACROS ========================================*/
#define DEFAULT_STATE_VALUE 70
#define KVKEY_GAME_PET_STATE "GAME_PET_STATE"
#define DPID_HAPPINESS 102
#define DPID_CLEANNESS 103
#define DPID_HEALTH 104
#define DPID_ENERGY 105
#define DPID_MOOD 107
#define PET_EVENT_TIMER 14
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

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
#endif

/*============================ IMPLEMENTATION ================================*/
// display pet state on lvgl
void _display_pet_state(ai_pet_state_t pet_state)
{
    switch (pet_state) {
        case AI_PET_STATE_EAT: {
            ai_audio_player_play_alert(AI_AUDIO_LOADING_TONE);
        } break;
        case AI_PET_STATE_BATH: {
            ai_audio_player_play_alert(AI_AUDIO_CANCEL_FAIL_TRI_TONE);
        } break;
        case AI_PET_STATE_TOILET: {
            ai_audio_player_play_alert(AI_AUDIO_CONFIRM);
        } break;
        case PET_EVENT_TAKE_BATH: {
            ai_audio_player_play_alert(AI_AUDIO_DOWNWARD_BI_TONE);
        } break;
        case AI_PET_STATE_SLEEP: {
            ai_audio_player_play_alert(AI_AUDIO_FAIL_CANCEL_BI_TONE);
        } break;
        default:
            break;
    }

    lv_vendor_disp_lock();
    pet_area_set_animation(pet_state);
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

    menu_system_update_pet_stats(&menu_state);
    return OPRT_OK;
}

// set data
OPERATE_RET game_pet_data_add(game_pet_state_id_t idx, int value)
{
    if (s_pet_state == NULL || idx < 0 || idx >= PET_STATE_TOTAL) {
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
    if (s_pet_state == NULL || idx < 0 || idx >= PET_OPT_TOTAL) {
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
    int rand_value =  tal_system_get_random(50);
    int rand_state = rand_value % PET_STATE_TOTAL;
    int rand_operation = rand_value % 2; // 0-add, 1-subtract
    rand_value = (rand_operation == 0) ? rand_value : -rand_value;

    PR_DEBUG("Random state [%d] updated: %d.", rand_state, rand_value);

    game_pet_data_add(rand_state, rand_value);

    return OPRT_OK;
}

 static void pet_event_callback(pet_event_type_t event_type, void *user_data)
 {
    if (event_type < 0 || event_type >= PET_EVENT_MAX) {
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
    if (s_pet_timer_once_id == timer_id) {
        PR_NOTICE("pet timer once callback");
        game_pet_show(s_pet_state);
    } else if (s_pet_timer_cycle_id == timer_id) {
        PR_NOTICE("pet timer cycle callback");
        game_pet_operation(PET_EVENT_TIMER, true);
    }
}

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
static void __app_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    PR_DEBUG("pet button function cb, mode: %d", event);
    switch (event) {
    case TDL_BUTTON_LONG_PRESS_START: {
        game_pet_reset();
    } break;
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        game_pet_random_state();
    } break;
    default:
        break;
    }
}

static OPERATE_RET __app_open_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_4, &button_cfg, &sg_button_hdl));

    // tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_LONG_PRESS_START, __app_button_function_cb);

    return rt;
}
#endif

// init
OPERATE_RET game_pet_init(void)
{
    // Initialize the game pet state
    OPERATE_RET rt = OPRT_OK;
    size_t readlen = 0;

    s_pet_state = (int *)tal_malloc(sizeof(int) * PET_STATE_TOTAL);
    if (NULL == s_pet_state) {
        return OPRT_MALLOC_FAILED;
    }

    // initialize state from KV storage or set to default values
    if ((OPRT_OK == tal_kv_get(KVKEY_GAME_PET_STATE, (uint8_t **)&s_pet_state, &readlen))
        && (readlen == sizeof(int) * PET_STATE_TOTAL)) {
        PR_INFO("Game pet initialized with KV state.");
    } else {
        game_pet_reset();
        PR_WARN("Game pet initialized with default state.");
    }

    // initialize timer
    TUYA_CALL_ERR_RETURN(tal_sw_timer_init());
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__timer_cb, NULL, &s_pet_timer_once_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__timer_cb, NULL, &s_pet_timer_cycle_id));
    TUYA_CALL_ERR_LOG(tal_sw_timer_start(s_pet_timer_cycle_id, PET_TIMER_CYCLE_MS, TAL_TIMER_CYCLE));

    menu_system_register_pet_event_callback(pet_event_callback, NULL);

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
    TUYA_CALL_ERR_RETURN(__app_open_button());
#endif

    return OPRT_OK;
}

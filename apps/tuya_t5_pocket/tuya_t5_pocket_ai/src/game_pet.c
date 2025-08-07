/**
 * @file game_pet.c
 * @brief
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
 
 // Pet event types for menu actions
typedef enum {
    PET_EVENT_FEED_HAMBURGER,
    PET_EVENT_DRINK_WATER,
    PET_EVENT_TOILET,
    PET_EVENT_TAKE_BATH,
    PET_EVENT_SEE_DOCTOR,
    PET_EVENT_SLEEP,
    PET_EVENT_WAKE_UP,
    PET_EVENT_COUNT
} pet_event_type_t;

// Pet event callback function type
typedef void (*pet_event_callback_t)(pet_event_type_t event_type, void *user_data);

/*============================ INCLUDES ======================================*/
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_kv.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "tal_sw_timer.h"
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
#define PET_EVENT_TIMER (PET_EVENT_COUNT)
#define PET_OPT_TOTAL (PET_EVENT_COUNT+1)

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
#define TAL_TIMER_CYCLE_MS (60000)  // 1000 * 60
#else
#define TAL_TIMER_CYCLE_MS (1200000)  // 1000 * 60 * 20
#endif

/*============================ LOCAL VARIABLES ===============================*/
static const int s_pet_opt_values[PET_OPT_TOTAL][PET_STATE_TOTAL] = {
    //                        health, energy, cleaness, happiness
    [PET_EVENT_FEED_HAMBURGER] = { 2,    10,    -1,     0},
    [PET_EVENT_DRINK_WATER]    = { 1,     5,    -2,     1},
    [PET_EVENT_TOILET]         = { 0,    -1,    -3,     1},
    [PET_EVENT_TAKE_BATH]      = { 0,    -2,    10,     3},
    [PET_EVENT_SEE_DOCTOR]     = {10,    -1,    -2,    -5},
    [PET_EVENT_SLEEP]          = { 3,    10,     0,     1},
    [PET_EVENT_WAKE_UP]        = { 1,    10,    -2,     2},
    [PET_EVENT_TIMER]          = {-1,    -3,    -2,    -4}
};

static const int s_pet_dp_values[PET_STATE_TOTAL] = {
    [PET_S_HEALTH_INDEX]     = DPID_HEALTH,
    [PET_S_ENERGY_INDEX]     = DPID_ENERGY,
    [PET_S_CLEAN_INDEX]      = DPID_CLEANNESS,
    [PET_S_HAPPINESS_INDEX]  = DPID_HAPPINESS
};

static TIMER_ID sw_timer_id = NULL;
static int *s_pet_state = NULL;

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
#endif

/*============================ IMPLEMENTATION ================================*/
// show
OPERATE_RET game_pet_show(int *state)
{
    if (state == NULL) {
        return OPRT_INVALID_PARM;
    }

    PR_INFO("Game Pet State - Health: %d, Energy: %d, Cleaness: %d, Happiness: %d",
            state[PET_S_HEALTH_INDEX], state[PET_S_ENERGY_INDEX],
            state[PET_S_CLEAN_INDEX], state[PET_S_HAPPINESS_INDEX]);

    int health = state[PET_S_HEALTH_INDEX];
    if (health < 10) {
        PR_DEBUG("Pet deadth.");
        return OPRT_OK;
    } else if (health < 30) {
        PR_DEBUG("Pet is ill.");
        return OPRT_OK;
    }

    int energy = state[PET_S_ENERGY_INDEX];
    if (energy < 30) {
        PR_DEBUG("Pet is hungry.");
        return OPRT_OK;
    } else if (energy > 80) {
        PR_DEBUG("Pet need exercise.");
        return OPRT_OK;
    }

    int clean = state[PET_S_CLEAN_INDEX];
    if (clean < 20) {
        PR_DEBUG("Pet is dirty.");
        return OPRT_OK;
    } else if (clean < 60) {
        PR_DEBUG("Pet need shower.");
        return OPRT_OK;
    }

    int happiness = state[PET_S_HAPPINESS_INDEX];
    if (happiness < 10) {
        PR_DEBUG("Pet is hopelessness.");
        return OPRT_OK;
    } else if (happiness < 50) {
        PR_DEBUG("Pet is sad.");
        return OPRT_OK;
    } else if (happiness > 80) {
        PR_DEBUG("Pet is very happy.");
        return OPRT_OK;
    } 

    PR_DEBUG("Pet is normal.");
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
    game_pet_show(s_pet_state);
    game_pet_data_report(s_pet_state);

    return OPRT_OK;
}

// pet operation
OPERATE_RET game_pet_operation(game_pet_opt_id_t idx)
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
    game_pet_show(s_pet_state);
    game_pet_data_report(s_pet_state);

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
    game_pet_show(s_pet_state);
    game_pet_data_report(s_pet_state);

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

static void __timer_cb(TIMER_ID timer_id, void *arg)
{
    PR_NOTICE("--- pet timer callback");
    game_pet_operation(PET_EVENT_TIMER);
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

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);
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

    if ((OPRT_OK == tal_kv_get(KVKEY_GAME_PET_STATE, (uint8_t **)&s_pet_state, &readlen))
        && (readlen == sizeof(int) * PET_STATE_TOTAL)) {
        PR_INFO("Game pet initialized with KV state.");
    } else {
        game_pet_reset();
        PR_WARN("Game pet initialized with default state.");
    }

    /* sw timer init & start */
    TUYA_CALL_ERR_RETURN(tal_sw_timer_init());
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__timer_cb, NULL, &sw_timer_id));
    TUYA_CALL_ERR_LOG(tal_sw_timer_start(sw_timer_id, TAL_TIMER_CYCLE_MS, TAL_TIMER_CYCLE));

#if defined(PET_DEBUG_ENABLE) && (PET_DEBUG_ENABLE == 1)
    TUYA_CALL_ERR_RETURN(__app_open_button());
#endif

    return OPRT_OK;
}

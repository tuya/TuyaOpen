#ifndef __GAME_PET_H__
#define __GAME_PET_H__

#include "lv_vendor.h"
#include "main_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_S_HEALTH_INDEX = 0,
    PET_S_ENERGY_INDEX,
    PET_S_CLEAN_INDEX,
    PET_S_HAPPINESS_INDEX,
    PET_STATE_TOTAL,
} game_pet_state_id_t;

typedef enum {
    MODE_DP_HAPPY = 0,
    MODE_DP_SAD,
    MODE_DP_EXCITED,
    MODE_DP_BORED,
    MODE_DP_ILL,
    MODE_DP_TOTAL,
} pet_mood_dp_value_t;


typedef enum {
    PET_ALERT_BI_TONE,
    PET_ALERT_CANCEL_FAIL_TRI_TONE,
    PET_ALERT_CONFIRM,
    PET_ALERT_DOWNWARD_BI_TONE,
    PET_ALERT_FAIL_CANCEL_BI_TONE,
    PET_ALERT_LOADING_TONE,
    PET_ALERT_SHORT_SELECT_TONE,
    PET_ALERT_THREE_STAGE_UP_TONE,
} PET_ALERT_TYPE_E;

typedef enum {
    AI_AUDIO_ALERT_NORMAL = 0,
    AI_AUDIO_ALERT_POWER_ON,
    AI_AUDIO_ALERT_NOT_ACTIVE,
    AI_AUDIO_ALERT_NETWORK_CFG,
    AI_AUDIO_ALERT_NETWORK_CONNECTED,
    AI_AUDIO_ALERT_NETWORK_FAIL,
    AI_AUDIO_ALERT_NETWORK_DISCONNECT,
    AI_AUDIO_ALERT_BATTERY_LOW,
    AI_AUDIO_ALERT_PLEASE_AGAIN,
    AI_AUDIO_ALERT_WAKEUP,
    AI_AUDIO_ALERT_LONG_KEY_TALK,
    AI_AUDIO_ALERT_KEY_TALK,
    AI_AUDIO_ALERT_WAKEUP_TALK,
    AI_AUDIO_ALERT_FREE_TALK,
} AI_AUDIO_ALERT_TYPE_E;

/**
 * @brief game pet operation function
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 */
OPERATE_RET game_pet_operation(pet_event_type_t idx, bool show_now);

/**
 * @brief game pet init function
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 *
 */
OPERATE_RET game_pet_init(void);

OPERATE_RET game_pet_reset(void);

OPERATE_RET game_pet_play_alert(PET_ALERT_TYPE_E type);

/**
 * @brief Plays an alert sound based on the specified alert type.
 *
 * @param type - The type of alert to play, defined by the APP_ALERT_TYPE_E enum.
 * @return OPERATE_RET - Returns OPRT_OK if the alert sound is successfully played, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_play_alert(AI_AUDIO_ALERT_TYPE_E type);

#ifdef __cplusplus
}
#endif

#endif // __GAME_PET_H__

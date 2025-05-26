/**
 * @file ai_audio.h
 * @version 0.1
 * @date 2025-04-15
 */

#include "tuya_cloud_types.h"
#include "ai_audio_agent.h"
#include "ai_audio_cloud_asr.h"
#include "ai_audio_player.h"
#include "ai_audio_input.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define GET_MIN_LEN(a, b) ((a) < (b) ? (a) : (b))

typedef uint8_t AI_AUDIO_WORK_MODE_E;
#define AI_AUDIO_MODE_MANUAL_SINGLE_TALK       1
#define AI_AUDIO_WORK_VAD_FREE_TALK            2
#define AI_AUDIO_WORK_ASR_WAKEUP_SINGLE_TALK   3
#define AI_AUDIO_WORK_ASR_WAKEUP_FREE_TALK     4

typedef enum {
    AI_AUDIO_EVT_NONE,
    AI_AUDIO_EVT_HUMAN_ASR_TEXT,
    AI_AUDIO_EVT_AI_REPLIES_TEXT_START,
    AI_AUDIO_EVT_AI_REPLIES_TEXT_DATA,
    AI_AUDIO_EVT_AI_REPLIES_TEXT_END,
    AI_AUDIO_EVT_AI_REPLIES_EMO,
    AI_AUDIO_EVT_ASR_WAKEUP,
} AI_AUDIO_EVENT_E;

typedef enum {
    AI_AUDIO_STATE_STANDBY,
    AI_AUDIO_STATE_LISTEN,
    AI_AUDIO_STATE_UPLOAD,
    AI_AUDIO_STATE_AI_SPEAK,
} AI_AUDIO_STATE_E;

typedef struct {
    char *name;
    char *text;
} AI_AUDIO_EMOTION_T;

typedef void (*AI_AUDIO_EVT_INFORM_CB)(AI_AUDIO_EVENT_E event, uint8_t *data, uint32_t len, void *arg);
typedef void (*AI_AUDIO_STATE_INFORM_CB)(AI_AUDIO_STATE_E state);

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef struct {
    AI_AUDIO_WORK_MODE_E      work_mode;
    AI_AUDIO_EVT_INFORM_CB    evt_inform_cb;
    AI_AUDIO_STATE_INFORM_CB  state_inform_cb;
} AI_AUDIO_CONFIG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Initializes the audio module with the provided configuration.
 * @param cfg Pointer to the configuration structure for the audio module.
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_init(AI_AUDIO_CONFIG_T *cfg);

/**
 * @brief Sets the volume for the audio module.
 * @param volume The volume level to set.
 * @return OPERATE_RET - OPRT_OK if the volume is set successfully, otherwise an error code.
 */
OPERATE_RET ai_audio_set_volume(uint8_t volume);

/**
 * @brief Retrieves the current volume setting for the audio module.
 * @param None
 * @return uint8_t - The current volume level.
 */
uint8_t ai_audio_get_volume(void);

/**
 * @brief Sets the open state of the audio module.
 * @param is_open Boolean value indicating whether to open (true) or close (false) the audio module.
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_set_open(bool is_open);

OPERATE_RET ai_audio_manual_start_single_talk(void);

OPERATE_RET ai_audio_manual_stop_single_talk(void);

OPERATE_RET ai_audio_set_wakeup(void);

AI_AUDIO_STATE_E ai_audio_get_state(void);
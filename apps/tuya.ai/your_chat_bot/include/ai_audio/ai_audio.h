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
typedef uint8_t  AI_AUDIO_WORK_MODE_E;
#define AI_AUDIO_WORK_MODE_HOLD         1
#define AI_AUDIO_WORK_MODE_TRIGGER      2
#define AI_AUDIO_WORK_MODE_WAKEUP       3
#define AI_AUDIO_WORK_MODE_FREE         4

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef struct {
    AI_AUDIO_WORK_MODE_E    work_mode;
    AI_AGENT_MSG_CB         agent_msg_cb;
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

/**
 * @brief Sets the work mode of the audio module.
 * @param work_mode The work mode to set for the audio module.
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_set_work_mode(AI_AUDIO_WORK_MODE_E work_mode);
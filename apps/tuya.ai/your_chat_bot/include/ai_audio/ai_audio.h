/**
 * @file ai_audio.h
 * @version 0.1
 * @date 2025-04-15
 */

#include "tuya_cloud_types.h"
#include "ai_audio_agent.h"
#include "ai_audio_cloud_asr.h"
#include "ai_audio_player.h"
#include "ai_audio_wakeup.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_open_vad;
    uint8_t is_open_asr;
    uint8_t is_enable_interrupt;
    uint32_t wakeup_timeout;

    AI_AGENT_MSG_CB agent_msg_cb;
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
OPERATE_RET ai_audio_init(AI_AUDIO_CONFIG_T *cfg);

OPERATE_RET ai_audio_set_volume(uint8_t volume);

uint8_t ai_audio_get_volume(void);

OPERATE_RET ai_audio_set_silent(bool is_silent);

bool ai_audio_is_silent(void);
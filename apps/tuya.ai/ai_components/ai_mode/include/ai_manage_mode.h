/**
 * @file ai_manage_mode.h
 * @brief AI mode management module header
 *
 * This header file defines the types and functions for managing different
 * AI chat modes (wakeup, oneshot, hold, free) and mode switching.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_MANAGE_MODE_H__
#define __AI_MANAGE_MODE_H__

#include "tuya_cloud_types.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_input.h"
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

#include "ai_user_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_CHAT_MODE_HOLD,       
    AI_CHAT_MODE_ONE_SHOT,    
    AI_CHAT_MODE_WAKEUP,      
    AI_CHAT_MODE_FREE,       

    AI_CHAT_MODE_CUSTOM_START = 0x100,   
} AI_CHAT_MODE_E;

typedef enum {
    AI_MODE_STATE_INIT,
    AI_MODE_STATE_IDLE,
    AI_MODE_STATE_LISTEN,
    AI_MODE_STATE_UPLOAD,
    AI_MODE_STATE_THINK,
    AI_MODE_STATE_SPEAK,
    AI_MODE_STATE_INVALID,
} AI_MODE_STATE_E;

typedef struct {
    const char *name;

    OPERATE_RET     (*init)         (void);
    OPERATE_RET     (*deinit)       (void);
    OPERATE_RET     (*task)         (void *args);
    OPERATE_RET     (*handle_event) (AI_NOTIFY_EVENT_T *event);
    AI_MODE_STATE_E (*get_state)    (void);
    OPERATE_RET     (*client_run)   (void *data);

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    OPERATE_RET     (*vad_change)   (AI_AUDIO_VAD_STATE_E vad_state);
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    OPERATE_RET   (*handle_key)  (TDL_BUTTON_TOUCH_EVENT_E event, void *arg);
#endif

}AI_MODE_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
@brief Register an AI chat mode
@param mode Chat mode to register
@param handle Pointer to mode handle structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_register(AI_CHAT_MODE_E mode, AI_MODE_HANDLE_T *handle);

/**
@brief Initialize a chat mode
@param mode Chat mode to initialize
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_init(AI_CHAT_MODE_E mode);

/**
@brief Deinitialize current chat mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_deinit(void);

/**
@brief Run current mode task
@param args Task arguments
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_task_running(void *args);

/**
@brief Handle AI user event
@param event Pointer to event structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_handle_event(AI_NOTIFY_EVENT_T *event);

/**
@brief Get current mode state
@return AI_MODE_STATE_E Current mode state
*/
AI_MODE_STATE_E ai_mode_get_state(void);

/**
@brief Run client callback for current mode
@param data Client data pointer
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_client_run(void *data);

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)

/**
@brief Handle VAD (Voice Activity Detection) state change for current mode
@param vad_state VAD state value
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_vad_change(AI_AUDIO_VAD_STATE_E vad_state);
#endif


#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
/**
@brief Handle button key event
@param event Button touch event
@param arg Callback argument
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_handle_key(TDL_BUTTON_TOUCH_EVENT_E event, void *arg);
#endif

/**
@brief Get current chat mode
@param mode Pointer to store current mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_get_curr_mode(AI_CHAT_MODE_E *mode);

/**
@brief Switch to a different chat mode
@param mode Target chat mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_switch(AI_CHAT_MODE_E mode);

/**
@brief Switch to next chat mode in the list
@return AI_CHAT_MODE_E Next mode value
*/
AI_CHAT_MODE_E ai_mode_switch_next(void);

/**
@brief Get mode state string
@param state Mode state
@return char* State string
*/
char *ai_get_mode_state_str(AI_MODE_STATE_E state);

/**
@brief Get mode name string
@param mode Mode 
@return char* name string
*/
char *ai_get_mode_name_str(AI_CHAT_MODE_E mode);

#ifdef __cplusplus
}
#endif

#endif /* __AI_MANAGE_MODE_H__ */

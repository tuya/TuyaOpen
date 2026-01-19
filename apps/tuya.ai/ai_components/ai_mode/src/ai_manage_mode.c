/**
 * @file ai_manage_mode.c
 * @brief AI mode management module implementation
 *
 * This module manages different AI chat modes (wakeup, oneshot, hold, free)
 * and provides functions for mode switching and event handling.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tuya_list.h"

#include "tal_api.h"

#include "ai_user_event.h"
#include "ai_manage_mode.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {                
    struct tuya_list_head  node;
    uint32_t               mode;
    MUTEX_HANDLE           mutex;
    AI_MODE_HANDLE_T       handle;
}AI_MODE_CTRL_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static struct tuya_list_head  sg_ai_mode_list = LIST_HEAD_INIT(sg_ai_mode_list);
static AI_MODE_CTRL_T   *sg_curr_mode_ctrl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static AI_MODE_CTRL_T *__find_chat_mode_ctrl(AI_CHAT_MODE_E mode)
{
    AI_MODE_CTRL_T *mode_ctrl = NULL;
    struct tuya_list_head *pos = NULL;

    tuya_list_for_each(pos, &sg_ai_mode_list) {
        mode_ctrl = tuya_list_entry(pos, AI_MODE_CTRL_T, node);
        if (mode_ctrl->mode == mode) {
            return mode_ctrl;
        }
    }

    return NULL;
}

/**
@brief Register an AI chat mode
@param mode Chat mode to register
@param handle Pointer to mode handle structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_register(AI_CHAT_MODE_E mode, AI_MODE_HANDLE_T *handle)
{
    AI_MODE_CTRL_T *mode_ctrl = NULL;

    mode_ctrl = __find_chat_mode_ctrl(mode);
    if(mode_ctrl) {
        PR_ERR("chat mode %d has been registered", mode);
        return OPRT_COM_ERROR;
    }

    NEW_LIST_NODE(AI_MODE_CTRL_T, mode_ctrl);
    if (NULL == mode_ctrl) {
        return OPRT_MALLOC_FAILED;
    }
    memset(mode_ctrl, 0, sizeof(AI_MODE_CTRL_T));

    mode_ctrl->mode = mode;
    memcpy(&mode_ctrl->handle, handle, sizeof(AI_MODE_HANDLE_T));

    tuya_list_add_tail(&mode_ctrl->node, &sg_ai_mode_list);

    PR_DEBUG("ai chat mode register mode %d success", mode);
    
    return OPRT_OK;
}

/**
@brief Initialize a chat mode
@param mode Chat mode to initialize
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_init(AI_CHAT_MODE_E mode)
{
    OPERATE_RET rt = OPRT_OK;
    AI_MODE_CTRL_T *mode_ctrl = NULL;

    mode_ctrl = __find_chat_mode_ctrl(mode);
    if(!mode_ctrl) {
        PR_ERR("chat mode %d not registered", mode);
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&mode_ctrl->mutex));

    if(mode_ctrl->handle.init) {
        TUYA_CALL_ERR_RETURN(mode_ctrl->handle.init());
    }

    sg_curr_mode_ctrl = mode_ctrl;

    ai_user_event_notify(AI_USER_EVT_MODE_SWITCH, (void *)mode);  

    return rt;
}

/**
@brief Deinitialize current chat mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_curr_mode_ctrl) {
        if(sg_curr_mode_ctrl->handle.deinit) {
            TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.deinit());
        }

        tal_mutex_release(sg_curr_mode_ctrl->mutex);
        sg_curr_mode_ctrl = NULL;
    }

    return rt;
}

/**
@brief Run current mode task
@param args Task arguments
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_task_running(void *args)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.task) {
        TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.task(args));
    }

    return rt;
}

/**
@brief Handle AI user event
@param event Pointer to event structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_handle_event(AI_NOTIFY_EVENT_T *event)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.handle_event) {
        TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.handle_event(event));
    }

    return rt;
}

/**
@brief Get current mode state
@return AI_MODE_STATE_E Current mode state
*/
AI_MODE_STATE_E ai_mode_get_state(void)
{
    AI_MODE_STATE_E state = AI_MODE_STATE_INVALID;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.get_state) {
        state = sg_curr_mode_ctrl->handle.get_state();
    }

    return state;
}

/**
@brief Run client callback for current mode
@param data Client data pointer
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_client_run(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.client_run) {
        TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.client_run(data));
    }

    return rt;
}

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
/**
@brief Handle VAD (Voice Activity Detection) state change for current mode
@param vad_state VAD state value
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_vad_change(AI_AUDIO_VAD_STATE_E vad_state)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.vad_change) {
        TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.vad_change(vad_state));
    }

    return rt;
}
#endif



#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
/**
@brief Handle button key event
@param event Button touch event
@param arg Callback argument
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_handle_key(TDL_BUTTON_TOUCH_EVENT_E event, void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.handle_key) {
        TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.handle_key(event, arg));
    }

    return rt;
}
#endif

/**
@brief Get current chat mode
@param mode Pointer to store current mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_get_curr_mode(AI_CHAT_MODE_E *mode)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == mode) {
        return OPRT_INVALID_PARM;
    }

    if(sg_curr_mode_ctrl == NULL) {
        PR_ERR("chat mode current mode is NULL");
        return OPRT_COM_ERROR;
    }

    *mode = sg_curr_mode_ctrl->mode;

    return rt;
}

/**
@brief Switch to a different chat mode
@param mode Target chat mode
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_mode_switch(AI_CHAT_MODE_E mode)
{
    OPERATE_RET rt = OPRT_OK;
    AI_MODE_CTRL_T *mode_ctrl = NULL;

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->mode == mode) {
        PR_DEBUG("chat mode %d is current mode", mode);
        return OPRT_OK;
    }

    mode_ctrl = __find_chat_mode_ctrl(mode);
    if(!mode_ctrl) {
        PR_ERR("chat mode %d not registered", mode);
        return OPRT_COM_ERROR;
    }

    if(sg_curr_mode_ctrl && sg_curr_mode_ctrl->handle.deinit) {
        TUYA_CALL_ERR_RETURN(sg_curr_mode_ctrl->handle.deinit());
    }

    if(mode_ctrl->handle.init) {
        TUYA_CALL_ERR_RETURN(mode_ctrl->handle.init());
    }

    sg_curr_mode_ctrl = mode_ctrl;

    ai_user_event_notify(AI_USER_EVT_MODE_SWITCH, (void *)mode);  

    PR_DEBUG("ai chat mode switch to mode %d success", mode);

    return rt;
}

/**
@brief Switch to next chat mode in the list
@return AI_CHAT_MODE_E Next mode value
*/
AI_CHAT_MODE_E ai_mode_switch_next(void)
{
    AI_CHAT_MODE_E nxt_mode = 0;
    OPERATE_RET rt = OPRT_OK;
    AI_MODE_CTRL_T *nxt_mode_ctrl = NULL;

    if(tuya_list_empty(&sg_ai_mode_list)) {
        PR_ERR("chat mode list is empty");
        return 0;
    }

    if(sg_curr_mode_ctrl == NULL) {
        nxt_mode_ctrl = tuya_list_entry(sg_ai_mode_list.next, AI_MODE_CTRL_T, node);
    }else {
        struct tuya_list_head *next = sg_curr_mode_ctrl->node.next;
        if(next == &sg_ai_mode_list) {
            nxt_mode_ctrl = tuya_list_entry(sg_ai_mode_list.next, AI_MODE_CTRL_T, node);
        }else {
            nxt_mode_ctrl = tuya_list_entry(next, AI_MODE_CTRL_T, node);
        }
    }

    if(NULL == nxt_mode_ctrl) {
        PR_ERR("get next chat mode failed");
        return 0;
    }

    if(sg_curr_mode_ctrl) {
        nxt_mode = sg_curr_mode_ctrl->mode;
    }
    
    rt = ai_mode_switch(nxt_mode_ctrl->mode);
    if(rt != OPRT_OK) {
        PR_ERR("switch to next chat mode %d failed", nxt_mode_ctrl->mode);
        return nxt_mode;
    }

    nxt_mode = nxt_mode_ctrl->mode;

    return nxt_mode;
}

/**
@brief Get mode state string
@param state Mode state
@return char* State string
*/
char *ai_get_mode_state_str(AI_MODE_STATE_E state)
{
    static char *_state_str[] = {
        "INIT",
        "IDLE",
        "LISTEN",
        "UPLOAD",
        "THINK",
        "SPEAK",
        "UNKNOWN"
    };

    if(state < AI_MODE_STATE_INIT || state >= AI_MODE_STATE_INVALID) {
        state = AI_MODE_STATE_INVALID;
    }

    return _state_str[state];
}

/**
@brief Get mode name string
@param mode Mode 
@return char* name string
*/
char *ai_get_mode_name_str(AI_CHAT_MODE_E mode)
{
    AI_MODE_CTRL_T *mode_ctrl = NULL;

    mode_ctrl = __find_chat_mode_ctrl(mode);
    if(!mode_ctrl) {
        PR_ERR("chat mode %d not registered", mode);
        return NULL;
    }

    return (char *)mode_ctrl->handle.name;
}
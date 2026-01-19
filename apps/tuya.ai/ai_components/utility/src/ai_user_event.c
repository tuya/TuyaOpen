/**
 * @file ai_user_event.c
 * @brief AI user event notification module implementation
 *
 * This module provides functions for registering and notifying user events
 * in the AI system. It allows components to register callback functions
 * that will be called when specific AI events occur.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "ai_user_event.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_USER_EVENT_NOTIFY sg_event_notify_cb = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
@brief Register a callback function for AI user event notifications
@param cb Callback function pointer to be called when events occur
@return None
*/
void ai_user_event_notify_register(AI_USER_EVENT_NOTIFY cb)
{
    sg_event_notify_cb = cb;
}

/**
@brief Notify registered callback about an AI user event
@param type Event type to notify
@param data Pointer to event data (can be NULL)
@return None
*/
void ai_user_event_notify(AI_USER_EVT_TYPE_E type,  void *data)
{
    if (sg_event_notify_cb) {
        AI_NOTIFY_EVENT_T event;
        event.type = type;
        event.data = data;
        sg_event_notify_cb(&event);
    }
}

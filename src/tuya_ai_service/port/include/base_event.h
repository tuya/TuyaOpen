/**
 * @file base_event.h
 * @brief base_event module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BASE_EVENT_H__
#define __BASE_EVENT_H__

#include "tuya_cloud_types.h"

#include "base_event_info.h"
#include "tal_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ty_subscribe_event tal_event_subscribe
#define ty_unsubscribe_event tal_event_unsubscribe
#define ty_publish_event tal_event_publish

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


#ifdef __cplusplus
}
#endif

#endif /* __BASE_EVENT_H__ */

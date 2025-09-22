/**
 * @file tal_event.h
 * @brief Defines the event management system for Tuya IoT applications.
 *
 * This header file provides the definitions and API declarations for the event
 * management system used in Tuya IoT applications. It includes functionalities
 * for event publishing, subscribing, and unsubscribing, allowing components
 * within the application to communicate and synchronize through a flexible
 * event-driven architecture. The event system supports different types of
 * subscriptions, including normal, emergency, and one-time events, to cater to
 * various application needs.
 *
 * The event management system is built on top of Tuya's IoT platform,
 * leveraging the platform's robust infrastructure for efficient and reliable
 * event handling. This system is crucial for developing responsive and modular
 * IoT applications that can react to changes in the environment or internal
 * state.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_EVENT_H__
#define __TAL_EVENT_H__

#include "tuya_cloud_types.h"
#include "tuya_list.h"
#include "tal_event_info.h"
#include "tal_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief max length of event name
 *
 */
#ifndef EVENT_NAME_MAX_LEN
#define EVENT_NAME_MAX_LEN (16) // move to tuya_iot_config.h. use kconfig config. default is 16
#endif

/**
 * @brief max length of event description
 *
 */
#define EVENT_DESC_MAX_LEN (32)

/**
 * @brief subscriber type
 *
 */
typedef uint8_t SUBSCRIBE_TYPE_E;
#define SUBSCRIBE_TYPE_NORMAL    0 // normal type, dispatch by the subscribe order, remove when unsubscribe
#define SUBSCRIBE_TYPE_EMERGENCY 1 // emergency type, dispatch first, remove when unsubscribe
#define SUBSCRIBE_TYPE_ONETIME                                                                                         \
    2 // one time type, dispatch by the subscribe order, remove after first time
      // dispath

/**
 * @brief the event dispatch raw data
 *
 */
typedef struct {
    int type;      // the data type
    int len;       // the data length
    char value[0]; // the data content
} EVENT_RAW_DATA_T;

/**
 * @brief event subscribe callback function type
 *
 */
typedef int (*EVENT_SUBSCRIBE_CB)(void *data);

/**
 * @brief the subscirbe node
 *
 */
typedef struct {
    char name[EVENT_NAME_MAX_LEN + 1]; // name, used to record the the event info
    char desc[EVENT_DESC_MAX_LEN + 1]; // description, used to record the subscribe info
    SUBSCRIBE_TYPE_E type;             // the subscribe type
    EVENT_SUBSCRIBE_CB cb;             // the subscribe callback function
    struct tuya_list_head node;        // list node, used to attch to the event node
} SUBSCRIBE_NODE_T;

/**
 * @brief the event node
 *
 */
typedef struct {
    MUTEX_HANDLE mutex; // mutex, protection the event publish and subscribe

    char name[EVENT_NAME_MAX_LEN + 1];    // name, the event name
    struct tuya_list_head node;           // list node, used to attach to the event manage module
    struct tuya_list_head subscribe_root; // subscibe root, used to manage the subscriber
} EVENT_NODE_T;

/**
 * @brief the event manage node
 *
 */
typedef struct {
    int inited;
    MUTEX_HANDLE mutex;                        // mutex, used to protection event manage node
    int event_cnt;                             // current event number
    struct tuya_list_head event_root;          // event root, used to manage the event
    struct tuya_list_head free_subscribe_root; // free subscriber list, used to manage the
                                               // subscribe which not found the event
} EVENT_MANAGE_T;

/**
 * @brief event initialization
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_event_init(void);

/**
 * @brief: publish event
 *
 * @param[in] name: event name
 * @param[in] data: event data
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_event_publish(const char *name, void *data);

/**
 * @brief: subscribe event
 *
 * @param[in] name: event name
 * @param[in] desc: subscribe description
 * @param[in] cb: subscribe callback function
 * @param[in] type: subscribe type
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_event_subscribe(const char *name, const char *desc, const EVENT_SUBSCRIBE_CB cb, SUBSCRIBE_TYPE_E type);

/**
 * @brief: unsubscribe event
 *
 * @param[in] name: event name
 * @param[in] desc: subscribe description
 * @param[in] cb: subscribe callback function
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tal_event_unsubscribe(const char *name, const char *desc, EVENT_SUBSCRIBE_CB cb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__TAL_EVENT_H__ */

/**
 * @file media_src.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __MEDIA_SRC_H__
#define __MEDIA_SRC_H__

#include "tuya_cloud_types.h"

#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)
#include "local_alert_src.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)

#if defined(ENABLE_AI_LANGUAGE_CHINESE) && (ENABLE_AI_LANGUAGE_CHINESE == 1)
#define LOCAL_ALERT_SRC_POWER_ON                media_src_prologue_zh
#define LOCAL_ALERT_SRC_NOT_ACTIVE              media_src_network_conn_zh
#define LOCAL_ALERT_SRC_NET_CFG                 media_src_network_config_zh
#define LOCAL_ALERT_SRC_NET_CONNECTED           media_src_network_conn_success_zh
#define LOCAL_ALERT_SRC_NET_FAILED              media_src_network_conn_failed_zh
#define LOCAL_ALERT_SRC_NET_DISCONNECT          media_src_network_reconfigure_zh
#define LOCAL_ALERT_SRC_LOW_BATTERY             media_src_low_battery_zh
#define LOCAL_ALERT_SRC_PLEASE_AGAIN            media_src_please_again_zh
#define LOCAL_ALERT_SRC_LONG_KEY_TALK           media_src_long_press_zh
#define LOCAL_ALERT_SRC_KEY_TALK                media_src_press_talk_zh
#define LOCAL_ALERT_SRC_WAKEUP_TALK             media_src_wakeup_chat_zh
#define LOCAL_ALERT_SRC_FREE_TALK               media_src_free_chat_zh
#define LOCAL_ALERT_SRC_WAKEUP                  media_src_ai_zh
 
#elif defined(ENABLE_AI_LANGUAGE_ENGLISH) && (ENABLE_AI_LANGUAGE_ENGLISH == 1)
#define LOCAL_ALERT_SRC_POWER_ON                media_src_prologue_en
#define LOCAL_ALERT_SRC_NOT_ACTIVE              media_src_network_conn_en
#define LOCAL_ALERT_SRC_NET_CFG                 media_src_network_config_en
#define LOCAL_ALERT_SRC_NET_CONNECTED           media_src_network_conn_success_en
#define LOCAL_ALERT_SRC_NET_FAILED              media_src_network_conn_failed_en
#define LOCAL_ALERT_SRC_NET_DISCONNECT          media_src_network_reconfigure_en
#define LOCAL_ALERT_SRC_LOW_BATTERY             media_src_low_battery_en
#define LOCAL_ALERT_SRC_PLEASE_AGAIN            media_src_please_again_en
#define LOCAL_ALERT_SRC_LONG_KEY_TALK           media_src_long_press_en
#define LOCAL_ALERT_SRC_KEY_TALK                media_src_press_talk_en
#define LOCAL_ALERT_SRC_WAKEUP_TALK             media_src_wakeup_chat_en
#define LOCAL_ALERT_SRC_FREE_TALK               media_src_free_chat_en
#define LOCAL_ALERT_SRC_WAKEUP                  media_src_ai_en

#endif

#endif

/***********************************************************
***********************extern define***********************
***********************************************************/
extern const uint8_t media_src_dingdong[3537];

/***********************************************************
********************function declaration********************
***********************************************************/




#ifdef __cplusplus
}
#endif

#endif /* __LOCAL_MEDIA_SRC_H__ */
